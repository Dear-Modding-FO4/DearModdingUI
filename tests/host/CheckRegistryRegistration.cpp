#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/host/Registry.h>
#include <DearModdingUI/navigation/Sidebar.h>
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

	void run_registry_registration_checks(Runner& runner)
	{
		runner.test("swapchain handoff requires a registered renderer replacement client", [] {
			Registry registry;
			CallbackState state;
			const auto regular = AddClient(registry, "regular.mod", "Regular", state);
			require(registry.ValidateSwapChainClient(regular) ==
					DMUI_RESULT_CLIENT_CAPABILITY_REQUIRED,
				"a regular client gained renderer replacement access");
			require(registry.ValidateSwapChainClient(9999) == DMUI_RESULT_CLIENT_NOT_FOUND,
				"an unknown client gained renderer replacement access");

			auto renderer = Client("renderer.mod", "Renderer", state);
			renderer.capabilities = DMUI_CLIENT_CAPABILITY_RENDERER_REPLACEMENT;
			DMUI_ClientHandle rendererHandle{};
			require(registry.RegisterClient(
						&renderer, &rendererHandle) == DMUI_RESULT_OK,
				"renderer replacement client was rejected");
			require(registry.ValidateSwapChainClient(rendererHandle) == DMUI_RESULT_OK,
				"renderer replacement capability was not retained");

			auto unknown = Client("unknown.mod", "Unknown", state);
			unknown.capabilities = 0x80000000u;
			DMUI_ClientHandle unknownHandle{};
			require(registry.RegisterClient(
						&unknown, &unknownHandle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"unknown client capabilities were accepted");
		});

		runner.test("duplicate client and page IDs are rejected in their scopes", [] {
			Registry registry;
			CallbackState state;
			const auto first = AddClient(registry, "a.mod", "A", state);
			auto duplicate = Client("a.mod", "Other", state);
			DMUI_ClientHandle client{};
			require(registry.RegisterClient(&duplicate, &client) ==
					DMUI_RESULT_DUPLICATE_CLIENT_ID,
				"duplicate client ID was accepted");
			const auto second = AddClient(registry, "b.mod", "B", state);
			AddCategory(registry, first, "general", "General");
			AddCategory(registry, second, "general", "General");
			(void)AddPage(registry, first, "settings", "Settings", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			auto page = Page("settings", "Duplicate", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			DMUI_PageHandle pageHandle{};
			require(registry.RegisterPage(first, &page, &pageHandle) ==
					DMUI_RESULT_DUPLICATE_PAGE_ID,
				"duplicate page ID in one client was accepted");
			require(registry.RegisterPage(second, &page, &pageHandle) == DMUI_RESULT_OK,
				"same page ID in another client was rejected");
		});

		runner.test("action registration validates descriptors clients duplicates and freeze", [] {
			Registry registry;
			CallbackState state;
			const auto first = AddClient(
				registry, "actions.first", "First", state);
			const auto second = AddClient(
				registry, "actions.second", "Second", state);
			auto action = Action(
				"copy-diagnostics", "Copy diagnostics", "clipboard-text", 0, state);
			DMUI_ActionHandle handle{};

			require(registry.RegisterAction(first, nullptr, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null action descriptor was accepted");
			require(registry.RegisterAction(first, &action, nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null action output was accepted");
			require(registry.RegisterAction(
						DMUI_INVALID_CLIENT_HANDLE, &action, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"invalid action client handle was accepted");
			action.structSize = sizeof(action) - 1;
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short action descriptor was accepted");
			action.structSize = sizeof(action);
			action.callback = nullptr;
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"null action callback was accepted");
			action.callback = &Draw;
			action.id = "invalid action";
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"invalid action ID was accepted");
			action.id = "copy-diagnostics";
			action.displayLabel = "";
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"empty action label was accepted");
			action.displayLabel = "Copy diagnostics";
			require(registry.RegisterAction(9999, &action, &handle) ==
					DMUI_RESULT_CLIENT_NOT_FOUND,
				"action for an unknown client was accepted");
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_OK,
				"valid action was rejected");
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_DUPLICATE_ACTION_ID,
				"duplicate action ID in one client was accepted");
			require(registry.RegisterAction(second, &action, &handle) ==
					DMUI_RESULT_OK,
				"same action ID in another client was rejected");
			require(registry.Freeze(), "action registry did not freeze");
			action.id = "late";
			require(registry.RegisterAction(first, &action, &handle) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"action registration remained open after freeze");
		});

		runner.test("frame observer registration validates descriptors clients and freeze", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(
				registry, "observer.mod", "Observer", state);
			auto observer = FrameObserver(state);
			DMUI_FrameObserverHandle handle{ 99 };

			require(!registry.HasActiveFrameObservers(),
				"empty registry reported an active frame observer");
			require(registry.RegisterFrameObserver(client, nullptr, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT &&
					handle == 99,
				"null frame observer descriptor was accepted");
			require(registry.RegisterFrameObserver(client, &observer, nullptr) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"null frame observer output was accepted");
			require(registry.RegisterFrameObserver(
						DMUI_INVALID_CLIENT_HANDLE, &observer, &handle) ==
					DMUI_RESULT_INVALID_ARGUMENT,
				"invalid frame observer client handle was accepted");
			observer.structSize = sizeof(observer) - 1;
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_STRUCT_TOO_SMALL,
				"short frame observer descriptor was accepted");
			observer.structSize = sizeof(observer);
			observer.callback = nullptr;
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"null frame observer callback was accepted");
			observer.callback = &Draw;
			require(registry.RegisterFrameObserver(9999, &observer, &handle) ==
					DMUI_RESULT_CLIENT_NOT_FOUND,
				"frame observer for an unknown client was accepted");
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_OK &&
					handle != DMUI_INVALID_FRAME_OBSERVER_HANDLE &&
					registry.HasActiveFrameObservers(),
				"valid frame observer was rejected");
			require(registry.InvokeFrameObserver(handle) == DMUI_RESULT_OK &&
					state.draws == 1,
				"valid frame observer was not dispatched");
			require(registry.Freeze(), "frame observer registry did not freeze");
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"frame observer registration remained open after freeze");
		});

		runner.test("frame observer dispatch contains and disables callback failures", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(
				registry, "observer.mod", "Observer", state);
			auto observer = FrameObserver(state);
			observer.callback = &ThrowDraw;
			DMUI_FrameObserverHandle handle{};
			require(registry.RegisterFrameObserver(client, &observer, &handle) ==
					DMUI_RESULT_OK,
				"throwing frame observer registration failed");
			require(registry.InvokeFrameObserver(handle) == DMUI_RESULT_CALLBACK_FAILED,
				"throwing frame observer escaped its guard");
			require(!registry.HasActiveFrameObservers(),
				"failed frame observer remained active");
			require(registry.InvokeFrameObserver(handle) == DMUI_RESULT_CALLBACK_FAILED,
				"failed frame observer was invoked again");
		});

		runner.test("categories require unique client-scoped stable IDs", [] {
			Registry registry;
			CallbackState state;
			const auto first =
				AddClient(registry, "first.categories", "First", state);
			const auto second =
				AddClient(registry, "second.categories", "Second", state);
			AddCategory(registry, first, "general", "General");
			AddCategory(registry, second, "general", "Other General");
			AddCategory(registry, second, "second-only", "Second Only");

			const DMUI_CategoryDescriptor duplicate{
				sizeof(DMUI_CategoryDescriptor),
				"general",
				"Renamed",
				99,
				0
			};
			require(
				registry.RegisterCategory(first, &duplicate) ==
					DMUI_RESULT_DUPLICATE_CATEGORY_ID,
				"duplicate category ID was merged or replaced");
			auto invalid = duplicate;
			invalid.id = "bad id";
			require(
				registry.RegisterCategory(first, &invalid) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"category ID whitespace was accepted");
			invalid.id = "valid";
			invalid.displayName = " \t ";
			require(
				registry.RegisterCategory(first, &invalid) ==
					DMUI_RESULT_INVALID_DESCRIPTOR,
				"whitespace-only category label was accepted");

			auto unknown = Page(
				"unknown", "Unknown", "missing", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			DMUI_PageHandle page{};
			require(
				registry.RegisterPage(first, &unknown, &page) ==
					DMUI_RESULT_CATEGORY_NOT_FOUND,
				"unknown category reference was accepted");
			unknown.categoryId = "second-only";
			require(
				registry.RegisterPage(first, &unknown, &page) ==
					DMUI_RESULT_CATEGORY_NOT_FOUND,
				"another client's category reference was accepted");
			auto crossClient = Page(
				"cross", "Cross", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(
				registry.RegisterPage(first, &crossClient, &page) ==
					DMUI_RESULT_OK,
				"owned category reference was rejected");
			require(registry.Freeze(), "category registry did not freeze");
			require(
				registry.RegisterCategory(first, &duplicate) ==
					DMUI_RESULT_REGISTRATION_CLOSED,
				"category registration remained open after freeze");
		});

		runner.test("category ordering uses sort key display name and stable ID", [] {
			Registry registry;
			CallbackState state;
			const auto client =
				AddClient(registry, "ordered.categories", "Ordered", state);
			AddCategory(registry, client, "General-10", "General", 10);
			AddCategory(registry, client, "Diagnostics11", "Diagnostics", 10);
			AddCategory(registry, client, "alpha", "Alpha");
			AddCategory(registry, client, "Diagnostics10", "Diagnostics", 10);
			(void)AddPage(
				registry, client, "uncategorized", "Uncategorized", nullptr, 100,
				DMUI_PAGE_KIND_SETTINGS, state);
			(void)AddPage(
				registry, client, "general", "General Page", "General-10", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			(void)AddPage(
				registry, client, "alpha", "Alpha Page", "alpha", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto diagnosticsPage = AddPage(
				registry, client, "diagnostics-a", "Diagnostics A", "Diagnostics10", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			(void)AddPage(
				registry, client, "diagnostics-b", "Diagnostics B", "Diagnostics11", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(registry.Freeze(), "ordered category registry did not freeze");
			const auto& categories = registry.Navigation().clients.front().categories;
			require(
				categories.size() == 5 &&
					categories[0].id.empty() &&
					categories[1].id == "alpha" &&
					categories[2].id == "Diagnostics10" &&
					categories[3].id == "Diagnostics11" &&
					categories[4].id == "General-10",
				"category ordering or stable identity changed");
			require(
				categories[2].displayName == categories[3].displayName &&
					SidebarCategoryKey(
						registry.Navigation().clients.front(),
						categories[2].id) !=
						SidebarCategoryKey(
							registry.Navigation().clients.front(),
							categories[3].id),
				"equal category labels collapsed stable expansion identities");
			ClientSelectionState selection{
				client,
				diagnosticsPage
			};
			for (const auto layout : {
					 SidebarLayoutKind::Tree,
					 SidebarLayoutKind::TwoPane,
					 SidebarLayoutKind::DrillDown,
					 SidebarLayoutKind::IconRail })
			{
				SidebarBrowsingState browsing;
				RevealSidebarSelection(layout, registry.Navigation(), selection, browsing);
				require(
					browsing.categoryExpansion[
						"ordered.categories/Diagnostics10"],
					"a sidebar layout used the category label as expansion identity");
			}
		});

		runner.test("registration copies strings and grows beyond the old capacity", [] {
			Registry registry;
			CallbackState state;
			char clientId[] = "copy.mod";
			char clientName[] = "Copy";
			auto clientDescriptor = Client(clientId, clientName, state);
			DMUI_ClientHandle client{};
			require(registry.RegisterClient(
						&clientDescriptor, &client) == DMUI_RESULT_OK,
				"copy client failed");
			clientId[0] = 'x';
			clientName[0] = 'X';
			char actionId[] = "copy";
			char actionLabel[] = "Copy diagnostics";
			char actionIcon[] = "clipboard-text";
			char actionTooltip[] = "Copy a summary.";
			auto action = Action(actionId, actionLabel, actionIcon, 0, state);
			action.tooltip = actionTooltip;
			DMUI_ActionHandle actionHandle{};
			require(registry.RegisterAction(client, &action, &actionHandle) ==
					DMUI_RESULT_OK,
				"copy action failed");
			AddCategory(registry, client, "general", "General");
			actionId[0] = 'x';
			actionLabel[0] = 'X';
			actionIcon[0] = 'x';
			actionTooltip[0] = 'X';
			(void)AddAction(
				registry, client, "zulu", "Zulu", nullptr, 10, state);
			(void)AddAction(
				registry, client, "bravo", "Bravo", nullptr, -10, state);
			(void)AddAction(
				registry, client, "alpha", "Alpha", nullptr, 10, state);
			for (size_t index = 0; index < 32; ++index)
			{
				const auto id = "page-" + std::to_string(index);
				const auto name = "Page " + std::to_string(index);
				(void)AddPage(registry, client, id.c_str(), name.c_str(), "general",
					static_cast<int32_t>(index), DMUI_PAGE_KIND_SETTINGS, state);
			}
			require(registry.Freeze(), "registry did not freeze");
			require(registry.PageCount() == 32, "dynamic registry retained a fixed capacity");
			require(registry.OrderedPages().front().clientId == "copy.mod",
				"client ID was not copied");
			require(registry.OrderedPages().front().clientDisplayName == "Copy",
				"client name was not copied");
			const auto& actions = registry.OrderedActions();
			require(
					actions.size() == 4 &&
						actions[0].id == "bravo" &&
						actions[1].id == "copy" &&
						actions[2].id == "alpha" &&
						actions[3].id == "zulu",
				"actions did not order by sort key then stable ID");
			const auto copiedAction = std::ranges::find(
				actions,
				"copy",
				&RegisteredAction::id);
			require(
					copiedAction != actions.end() &&
						copiedAction->displayLabel ==
							"Copy diagnostics" &&
						copiedAction->iconName ==
							"clipboard-text" &&
						copiedAction->tooltip ==
							"Copy a summary.",
					"action descriptor strings were not copied");
		});

	}
}
