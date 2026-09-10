#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/host/Registry.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	void run_registry_lifecycle_checks(Runner& runner)
	{
		runner.test("registry freeze rejects late clients and pages", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(registry, "freeze.mod", "Freeze", state);
			require(registry.Freeze(), "registry did not freeze");
			auto lateClient = Client("late.mod", "Late", state);
			DMUI_ClientHandle clientHandle{};
			require(registry.RegisterClient(
						&lateClient, &clientHandle) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"late client was accepted");
			auto latePage = Page("late", "Late", "General", 0, DMUI_PAGE_KIND_SETTINGS, state);
			DMUI_PageHandle pageHandle{};
			require(registry.RegisterPage(client, &latePage, &pageHandle) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"late page was accepted");
		});

		runner.test("ready and unavailable notifications happen exactly once", [] {
			CallbackState readyState;
			Registry readyRegistry;
			(void)AddClient(readyRegistry, "ready.mod", "Ready", readyState);
			require(readyRegistry.Freeze(), "ready registry did not freeze");
			const DMUI_HostReadyInfo info{
				sizeof(DMUI_HostReadyInfo),
				DMUI_API_VERSION_CURRENT
			};
			readyRegistry.NotifyReady(info);
			readyRegistry.NotifyReady(info);
			readyRegistry.NotifyUnavailable(DMUI_UNAVAILABLE_BACKEND_FAILED);
			require(readyState.ready == 1 && readyState.unavailable == 0,
				"ready client received duplicate or mixed notifications");

			CallbackState unavailableState;
			Registry unavailableRegistry;
			(void)AddClient(
				unavailableRegistry, "fallback.mod", "Fallback", unavailableState);
			require(unavailableRegistry.Freeze(), "unavailable registry did not freeze");
			unavailableRegistry.NotifyUnavailable(DMUI_UNAVAILABLE_BACKEND_FAILED);
			unavailableRegistry.NotifyUnavailable(DMUI_UNAVAILABLE_HOST_DISABLED);
			unavailableRegistry.NotifyReady(info);
			require(unavailableState.ready == 0 && unavailableState.unavailable == 1,
				"unavailable client received duplicate or mixed notifications");
			require(unavailableState.reason == DMUI_UNAVAILABLE_BACKEND_FAILED,
				"unavailable reason changed");
		});

		runner.test("throwing client callbacks are isolated by host guards", [] {
			const DMUI_HostReadyInfo info{
				sizeof(DMUI_HostReadyInfo),
				DMUI_API_VERSION_CURRENT
			};

			CallbackState readyState;
			Registry readyRegistry;
			auto readyClient = Client("throw-ready.mod", "Throw Ready", readyState);
			readyClient.onHostReady = &ThrowReady;
			DMUI_ClientHandle readyHandle{};
			require(readyRegistry.RegisterClient(
						&readyClient, &readyHandle) == DMUI_RESULT_OK,
				"throwing ready client was not registered");
			AddCategory(readyRegistry, readyHandle, "general", "General");
			const auto readyPage = AddPage(
				readyRegistry,
				readyHandle,
				"settings",
				"Settings",
				"general",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				readyState);
			require(readyRegistry.Freeze(), "throwing ready registry did not freeze");
			readyRegistry.NotifyReady(info);
			require(readyRegistry.PageFailed(readyPage),
				"a client with a throwing ready callback remained drawable");

			CallbackState drawState;
			Registry drawRegistry;
			const auto drawClient = AddClient(
				drawRegistry, "throw-draw.mod", "Throw Draw", drawState);
			AddCategory(drawRegistry, drawClient, "general", "General");
			auto drawPageDescriptor = Page(
				"settings", "Settings", "general", 0, DMUI_PAGE_KIND_SETTINGS, drawState);
			drawPageDescriptor.draw = &ThrowDrawPage;
			DMUI_PageHandle drawPage{};
			require(drawRegistry.RegisterPage(
						drawClient, &drawPageDescriptor, &drawPage) == DMUI_RESULT_OK,
				"throwing draw page was not registered");
			require(drawRegistry.InvokePage(drawPage) == DMUI_RESULT_CALLBACK_FAILED,
				"a throwing page escaped its host guard");
			require(drawRegistry.InvokePage(drawPage) == DMUI_RESULT_CALLBACK_FAILED,
				"a faulted page was invoked again");

			auto drawActionDescriptor = Action(
				"throw", "Throw", nullptr, 0, drawState);
			drawActionDescriptor.callback = &ThrowDraw;
			DMUI_ActionHandle drawAction{};
			require(drawRegistry.RegisterAction(
						drawClient, &drawActionDescriptor, &drawAction) ==
					DMUI_RESULT_OK,
				"throwing action was not registered");
			require(drawRegistry.InvokeAction(drawAction) ==
					DMUI_RESULT_CALLBACK_FAILED,
				"a throwing action escaped its host guard");
			require(drawRegistry.ActionFailed(drawAction),
				"faulted action was not permanently disabled");
			require(drawRegistry.InvokeAction(drawAction) ==
					DMUI_RESULT_CALLBACK_FAILED,
				"a faulted action was invoked again");

			CallbackState unavailableState;
			CallbackState healthyState;
			Registry unavailableRegistry;
			auto unavailableClient = Client(
				"throw-unavailable.mod", "Throw Unavailable", unavailableState);
			unavailableClient.onHostUnavailable = &ThrowUnavailable;
			DMUI_ClientHandle unavailableHandle{};
			require(unavailableRegistry.RegisterClient(
						&unavailableClient,
						&unavailableHandle) == DMUI_RESULT_OK,
				"throwing unavailable client was not registered");
			(void)AddClient(
				unavailableRegistry, "healthy.mod", "Healthy", healthyState);
			require(unavailableRegistry.Freeze(), "unavailable registry did not freeze");
			unavailableRegistry.NotifyUnavailable(DMUI_UNAVAILABLE_BACKEND_FAILED);
			require(healthyState.unavailable == 1,
				"a throwing unavailable callback blocked the next client");
		});

		runner.test("overlay frame demand is reference counted and never makes settings demand frames", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(registry, "frames.mod", "Frames", state);
			AddCategory(registry, client, "general", "General");
			AddCategory(registry, client, "hud", "HUD");
			const auto settings = AddPage(registry, client, "settings", "Settings", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto overlay = AddPage(registry, client, "overlay", "Overlay", "hud", 0,
				DMUI_PAGE_KIND_OVERLAY, state);
			require(registry.RequestFrame(client, settings) == DMUI_RESULT_INVALID_PAGE_KIND,
				"settings page requested overlay frames");
			require(registry.RequestFrame(client, overlay) == DMUI_RESULT_OK,
				"first overlay request failed");
			require(registry.RequestFrame(client, overlay) == DMUI_RESULT_OK,
				"second overlay request failed");
			require(registry.DemandedOverlayCount() == 1, "one overlay counted twice");
			require(registry.ReleaseFrame(client, overlay) == DMUI_RESULT_OK,
				"first overlay release failed");
			require(registry.IsFrameDemanded(overlay), "one release cleared two requests");
			require(registry.ReleaseFrame(client, overlay) == DMUI_RESULT_OK,
				"second overlay release failed");
			require(!registry.IsFrameDemanded(overlay), "balanced releases left demand");
			require(registry.ReleaseFrame(client, overlay) == DMUI_RESULT_NO_FRAME_DEMAND,
				"unbalanced release was accepted");
			require(registry.HasSettingsPages(), "overlay behavior hid modal settings");
		});

		runner.test("page callbacks receive userdata and failed lookups stay isolated", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(registry, "draw.mod", "Draw", state);
			AddCategory(registry, client, "general", "General");
			const auto page = AddPage(registry, client, "draw", "Draw", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(registry.InvokePage(page) == DMUI_RESULT_OK, "draw callback failed");
			require(state.draws == 1, "draw callback did not receive userdata");
			require(registry.InvokePage(page + 1) == DMUI_RESULT_PAGE_NOT_FOUND,
				"unknown page was invoked");
			const auto action = AddAction(
				registry, client, "draw", "Draw", nullptr, 0, state);
			require(registry.InvokeAction(action) == DMUI_RESULT_OK,
				"action callback failed");
			require(state.draws == 2, "action callback did not receive userdata");
			require(registry.InvokeAction(action + 1) == DMUI_RESULT_ACTION_NOT_FOUND,
				"unknown action was invoked");
		});

		runner.test("page UI errors cross the callback boundary without becoming false draws", [] {
			Registry registry;
			CallbackState state;
			const auto client =
				AddClient(registry, "ui-error.mod", "UI error", state);
			AddCategory(registry, client, "general", "General");
			auto descriptor =
				Page("ui-error", "UI error", "general", 0,
					DMUI_PAGE_KIND_SETTINGS, state);
			descriptor.draw = &UnsupportedDrawPage;
			DMUI_PageHandle page{};
			require(
				registry.RegisterPage(client, &descriptor, &page) ==
					DMUI_RESULT_OK,
				"UI-error page registration failed");
			require(
				registry.InvokePage(page) == DMUI_RESULT_UNSUPPORTED_ABI &&
					registry.PageFailed(page) &&
					registry.InvokePage(page) == DMUI_RESULT_CALLBACK_FAILED,
				"UI error was not surfaced and isolated at the callback boundary");
		});
	}
}
