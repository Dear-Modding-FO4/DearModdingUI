#include "../support/DearModdingUITestSupport.h"
#include "../support/PresentationTestSupport.h"
#include <DearModdingUI/host/Registry.h>
#include <DearModdingUI/host/RenderExecution.h>
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
		runner.test("client actions have scoped UI authorization and propagate sticky failures", [] {
			Registry registry;
			CallbackState state;
			const auto handle = AddClient(registry, "actions.mod", "Actions", state);
			static Registry* activeRegistry;
			activeRegistry = &registry;
			DMUI_HostAPI api{};
			api.ui = &UI::API();
			api.registerAction = [](DMUI_ClientHandle a_client, const DMUI_ActionDescriptor* a_descriptor,
				DMUI_ActionHandle* a_action) noexcept {
				return activeRegistry->RegisterAction(a_client, a_descriptor, a_action);
			};
			api.reportDiagnostic = [](DMUI_ClientHandle, const DMUI_DiagnosticDescriptor*) noexcept {
				return DMUI_RESULT_OK;
			};
			dmui::Client client{ "actions.mod", "Actions", { 1, 0 } };
			dmui::detail::ClientTestAccess::Bind(client, api, handle);
			require(client.AddAction("copy", "Copy diagnostics", nullptr, nullptr,
				[] { dmui::ui::SetClipboardText("Diagnostics"); }), "copy action registration failed");
			require(client.AddAction("invalid", "Invalid clipboard", nullptr, nullptr,
				[] { dmui::ui::SetClipboardText(std::string_view{ nullptr, 1 }); }),
				"invalid action registration failed");
			require(client.AddAction("throws", "Throwing action", nullptr, nullptr,
				[] { throw 1; }), "throwing action registration failed");
			support::ImGuiTestContext imgui;
			std::string copied;
			auto& platform = ImGui::GetPlatformIO();
			platform.Platform_ClipboardUserData = &copied;
			platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* a_text) {
				*static_cast<std::string*>(ImGui::GetPlatformIO().Platform_ClipboardUserData) = a_text;
			};
			imgui.BeginWindow("##ActionUIContext");
			RenderExecution::Guard execution{ RenderExecution::Phase::kFrameDraw };
			(void)execution.NoteBinding(1);
			const RenderExecution::ClientGuard callback{ handle, true };
			const auto& actions = registry.OrderedActions();
			require(registry.InvokeAction(actions[0].handle) == DMUI_RESULT_OK &&
				client.LastResult() == DMUI_RESULT_OK && copied == "Diagnostics",
				"action could not copy through the real host UI adapter");
			require(registry.InvokeAction(actions[1].handle) == DMUI_RESULT_INVALID_ARGUMENT &&
				client.LastResult() == DMUI_RESULT_INVALID_ARGUMENT &&
				registry.ActionFailed(actions[1].handle) &&
				registry.InvokeAction(actions[1].handle) == DMUI_RESULT_CALLBACK_FAILED,
				"action sticky UI failure did not cross the callback boundary");
			require(registry.InvokeAction(actions[2].handle) == DMUI_RESULT_CALLBACK_FAILED &&
				client.LastResult() == DMUI_RESULT_CALLBACK_FAILED &&
				registry.ActionFailed(actions[2].handle),
				"action exception was swallowed instead of isolated");
			require(dmui::ui::detail::currentContext == nullptr && copied == "Diagnostics",
				"action leaked its UI context or invalid text overwrote the clipboard");
			imgui.EndWindow();
		});

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
				DMUI_ABI_VERSION
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
				DMUI_ABI_VERSION
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
			require(drawRegistry.PageFailed(drawPage) && drawRegistry.HasSettingsPages(),
				"faulting a page removed the host's settings shell");
			require(drawRegistry.InvokePage(drawPage + 1000) ==
						DMUI_RESULT_PAGE_NOT_FOUND &&
					drawRegistry.InvokeAction(DMUI_ActionHandle{ 1000 }) ==
						DMUI_RESULT_ACTION_NOT_FOUND,
				"an unknown handle was invoked");
			auto uiErrorDescriptor = Page(
				"ui-error", "UI error", "general", 1, DMUI_PAGE_KIND_SETTINGS, drawState);
			uiErrorDescriptor.draw = &UnsupportedDrawPage;
			DMUI_PageHandle uiErrorPage{};
			require(
				drawRegistry.RegisterPage(
					drawClient, &uiErrorDescriptor, &uiErrorPage) == DMUI_RESULT_OK &&
					drawRegistry.InvokePage(uiErrorPage) == DMUI_RESULT_UNSUPPORTED_ABI &&
					drawRegistry.PageFailed(uiErrorPage) &&
					drawRegistry.InvokePage(uiErrorPage) == DMUI_RESULT_CALLBACK_FAILED,
				"UI error was not surfaced and isolated at the callback boundary");

			auto drawActionDescriptor = Action(
				"throw", "Throw", nullptr, 0, drawState);
			drawActionDescriptor.callback = &ThrowDrawPage;
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
	}
}
