#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/navigation/NavigationController.h>
#include <DearModdingUI/navigation/NavigationPresentation.h>
#include <DearModdingUI/navigation/SidebarComparison.h>
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

	void run_navigation_checks(Runner& runner)
	{
		runner.test("navigation groups clients categories and settings pages deterministically", [] {
			Registry registry;
			CallbackState state;
			const auto bravo = AddClient(registry, "bravo.mod", "Bravo", state);
			const auto alpha = AddClient(
				registry, "alpha.mod", "Alpha", state);
			AddCategory(registry, alpha, "general", "General");
			AddCategory(registry, alpha, "advanced", "Advanced", -10);
			AddCategory(registry, alpha, "hud", "HUD");
			AddCategory(registry, bravo, "general", "General");
			const auto ungrouped = AddPage(
				registry, alpha, "overview", "Overview", nullptr, 100,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto alphaLate = AddPage(registry, alpha, "late", "Late", "general", 20,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto alphaEarly = AddPage(registry, alpha, "early", "Early", "general", -10,
				DMUI_PAGE_KIND_SETTINGS, state);
			(void)AddPage(registry, alpha, "advanced", "Advanced", "advanced", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			const auto overlay = AddPage(registry, alpha, "overlay", "Overlay", "hud", 0,
				DMUI_PAGE_KIND_OVERLAY, state);
			(void)AddPage(registry, bravo, "settings", "Settings", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(registry.Freeze(), "registry did not freeze");

			const auto& navigation = registry.Navigation();
			require(navigation.clients.size() == 2, "settings clients were not grouped");
			require(navigation.clients[0].id == "alpha.mod", "client order changed");
			require(navigation.clients[0].categories.size() == 3, "categories were not grouped");
			require(
				navigation.clients[0].categories[0].displayName.empty() &&
					navigation.clients[0].categories[0].pages[0].handle ==
						ungrouped,
				"uncategorized pages did not remain first");
			require(navigation.clients[0].categories[1].displayName == "Advanced",
				"category order changed");
			require(navigation.clients[0].categories[2].pages[0].handle == alphaEarly,
				"page sort key was ignored");
			require(navigation.clients[0].categories[2].pages[1].handle == alphaLate,
				"page sort order changed");
			require(navigation.FindPage(alphaEarly) != nullptr, "settings page was not indexed");
			require(navigation.FindPage(overlay) == nullptr,
				"overlay page entered settings navigation");
		});

		runner.test("navigation caches shared icon selections at model ownership", [] {
			Registry registry;
			CallbackState state;
			auto clientDescriptor = Client("wrench.mod", "Wrench Mod", state);
			clientDescriptor.iconName = "hammer";
			DMUI_ClientHandle client{};
			require(
				registry.RegisterClient(&clientDescriptor, &client) ==
					DMUI_RESULT_OK,
				"icon test client registration failed");
			AddCategory(
				registry,
				client,
				"wrench",
				"Wrench");
			const auto page = AddPage(
				registry,
				client,
				"repair",
				"Repairs",
				"wrench",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			const auto action = AddAction(
				registry,
				client,
				"repair-action",
				"Repairs",
				"unknown",
				0,
				state);
			require(registry.Freeze(), "icon test registry did not freeze");

			const auto wrench = FindPhosphorIconGlyphOrZero("wrench");
			const auto hammer = FindPhosphorIconGlyphOrZero("hammer");
			const auto& navigation = registry.Navigation();
			const auto* navigationClient = navigation.FindClient(client);
			const auto* navigationPage = navigation.FindPage(page);
			const auto actionRecord = std::ranges::find_if(
				navigation.SearchIndex(),
				[&](const auto& a_record) {
					return a_record.entry.kind ==
							NavigationItemKind::kAction &&
						a_record.entry.action == action;
				});
			require(
				navigationClient &&
					ResolveNavigationClientIconGlyph(*navigationClient) ==
						hammer &&
					navigationClient->categories.size() == 1 &&
					ResolveNavigationCategoryIconGlyph(
						navigationClient->categories.front()) == wrench,
				"client or category model selection lost its precedence");
			require(
				navigationPage &&
					navigationPage->iconSelection.HasSelection() &&
					navigationPage->iconSelection.glyph ==
						FindPhosphorIconGlyphOrZero("wrench"),
				"page model did not cache its semantic selection");
			require(
				actionRecord != navigation.SearchIndex().end() &&
					registry.OrderedActions().size() == 1 &&
					registry.OrderedActions().front().iconSelection.glyph ==
						wrench &&
					actionRecord->entry.iconSelection.HasSelection() &&
					actionRecord->entry.iconSelection.glyph == wrench &&
					ResolveNavigationSearchEntryGlyph(
						actionRecord->entry) == wrench,
				"toolbar and palette did not share the registered action selection");
		});

		runner.test("navigation sections preserve declared origin and exact source identity", [] {
			Registry registry;
			CallbackState state;
			const auto nativeZulu = AddClient(
				registry,
				"native.zulu",
				"Zulu Native",
				state);
			const auto nativeAlpha = AddClient(
				registry,
				"native.alpha",
				"Alpha Native",
				state);
			const auto unnamed = AddClient(
				registry,
				"bridge.unnamed",
				"Unnamed Bridge",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"");
			const auto mcmZulu = AddClient(
				registry,
				"bridge.mcm-zulu",
				"Zulu MCM",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"MCM");
			const auto mcmAlpha = AddClient(
				registry,
				"bridge.mcm-alpha",
				"Alpha MCM",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"MCM");
			const auto colliding = AddClient(
				registry,
				"bridge.native-label",
				"Native Label Bridge",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"Native");
			const auto noSettings = AddClient(
				registry,
				"bridge.no-settings",
				"No Settings",
				state,
				DMUI_CLIENT_ORIGIN_BRIDGED,
				"Unused");
			(void)nativeZulu;
			AddCategory(registry, mcmZulu, "overview-category", "Overview");
			(void)AddPage(
				registry,
				nativeAlpha,
				"overview",
				"Overview",
				nullptr,
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				unnamed,
				"overview",
				"Overview",
				nullptr,
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				mcmZulu,
				"overview",
				"Overview",
				"overview-category",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				mcmAlpha,
				"overview",
				"Overview",
				nullptr,
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				colliding,
				"overview",
				"Overview",
				nullptr,
				0,
				DMUI_PAGE_KIND_SETTINGS,
				state);
			(void)AddPage(
				registry,
				noSettings,
				"overlay",
				"Overlay",
				nullptr,
				0,
				DMUI_PAGE_KIND_OVERLAY,
				state);
			require(registry.Freeze(), "section registry did not freeze");

			const auto& navigation = registry.Navigation();
			const auto* nativeClient = navigation.FindClient(nativeAlpha);
			const auto* mcmClient = navigation.FindClient(mcmAlpha);
			const auto* equalCategoryClient = navigation.FindClient(mcmZulu);
			require(
				navigation.clients.size() == 5 &&
					nativeClient &&
					nativeClient->id == "native.alpha" &&
					nativeClient->origin ==
						DMUI_CLIENT_ORIGIN_NATIVE &&
					mcmClient &&
					mcmClient->id == "bridge.mcm-alpha" &&
					mcmClient->origin ==
						DMUI_CLIENT_ORIGIN_BRIDGED &&
					mcmClient->bridgeSourceLabel == "MCM",
				"navigation did not copy origin metadata or filter settings clients");

			const auto& sections = navigation.sections;
			require(
				sections.size() == 4 &&
					sections[0].origin == DMUI_CLIENT_ORIGIN_NATIVE &&
					sections[0].clientIndices.size() == 1 &&
					navigation.clients[sections[0].clientIndices[0]].id ==
						"native.alpha" &&
					sections[1].bridgeSourceLabel.empty() &&
					navigation.clients[sections[1].clientIndices[0]].id ==
						"bridge.unnamed" &&
					sections[2].bridgeSourceLabel == "MCM" &&
					sections[2].clientIndices.size() == 2 &&
					navigation.clients[sections[2].clientIndices[0]].id ==
						"bridge.mcm-alpha" &&
					navigation.clients[sections[2].clientIndices[1]].id ==
						"bridge.mcm-zulu" &&
					sections[3].bridgeSourceLabel == "Native" &&
					navigation.clients[sections[3].clientIndices[0]].id ==
						"bridge.native-label",
				"navigation sections lost deterministic source or client order");
			require(
				NavigationClientSectionLabel(
					DMUI_CLIENT_ORIGIN_NATIVE,
					{}) == "Native" &&
					NavigationClientSectionLabel(
						DMUI_CLIENT_ORIGIN_BRIDGED,
						{}) == "Bridged" &&
					NavigationClientSectionLabel(
						DMUI_CLIENT_ORIGIN_BRIDGED,
						"Native") == "Native",
				"navigation section labels changed factual source display");

			const auto bridged = DestinationsNavigationPresentation::Build(
				navigation,
				{ DMUI_CLIENT_ORIGIN_BRIDGED });
			require(
				bridged.sections.size() == 3 &&
					std::ranges::all_of(
						bridged.sections,
						[](const auto& a_section) {
							return a_section.showHeading;
						}),
				"origin filtering retained the other destination");
			require(
				mcmClient->categories[0].displayName.empty() &&
					equalCategoryClient &&
					equalCategoryClient->categories[0].displayName ==
						"Overview",
				"source grouping changed empty or display-name-equal categories");

			auto copied = navigation;
			auto moved = std::move(copied);
			require(
				moved.sections.size() == navigation.sections.size() &&
					moved.FindSectionForClient(mcmAlpha) &&
					moved.FindSectionForClient(mcmAlpha)
							->bridgeSourceLabel == "MCM" &&
					moved.clients[
						moved.FindSectionForClient(mcmAlpha)
							->clientIndices.front()]
							.handle == mcmAlpha,
				"navigation section membership did not survive copy and move");
		});

		runner.test("controlled source chrome follows external reveals across frames", [] {
			NavigationModel model;
			model.clients = {
				{
					1,
					"native",
					"Native",
					1,
					{ { "General", {
						{ 10, 1, "page", "Page", "General", {}, 0 }
					} } },
					{},
					DMUI_CLIENT_ORIGIN_NATIVE,
					{}
				},
				{
					2,
					"bridge",
					"Bridge",
					1,
					{ { "General", {
						{ 20, 2, "page", "Page", "General", {}, 0 }
					} } },
					{},
					DMUI_CLIENT_ORIGIN_BRIDGED,
					"MCM"
				}
			};
			model.sections = {
				{ DMUI_CLIENT_ORIGIN_NATIVE, {}, { 0 } },
				{ DMUI_CLIENT_ORIGIN_BRIDGED, "MCM", { 1 } }
			};
			ClientSelectionState selection;
			require(
				ApplyNavigationRequest(
					model,
					NavigationRequest::Page(10),
					selection).accepted,
				"initial native page selection failed");
			NavigationPresentationState presentationState;

			auto presentation = BuildNavigationPresentation(
				NavigationPresentationKind::Destinations,
				model,
				presentationState);
			const auto native = std::ranges::find(
				presentation.sourceControls,
				std::string_view{ "Native" },
				&NavigationSourceControl::label);
			const auto bridged = std::ranges::find(
				presentation.sourceControls,
				std::string_view{ "Bridged" },
				&NavigationSourceControl::label);
			require(
				native != presentation.sourceControls.end() &&
					bridged != presentation.sourceControls.end() &&
					native->selected &&
					ResolveControlledNavigationControlDecision(
						native->selected,
						false) ==
						ControlledNavigationControlDecision::None,
				"controlled source chrome treated active visibility as a click");

			require(
				ResolveControlledNavigationControlDecision(
					bridged->selected,
					true) ==
					ControlledNavigationControlDecision::Activate,
				"explicit source click did not request activation");
			const auto switchToBridged =
				ActivateNavigationSourceControl(
					NavigationPresentationKind::Destinations,
					model,
					bridged->id,
					selection,
					presentationState);
			require(
				switchToBridged.accepted &&
					switchToBridged.presentationChanged &&
					switchToBridged.navigation &&
					switchToBridged.navigation->kind ==
						NavigationRequestKind::Client &&
					switchToBridged.navigation->client == 2,
				"presentation did not own source fallback selection");
			require(
				ApplyNavigationRequest(
					model,
					*switchToBridged.navigation,
					selection).accepted,
				"source fallback request was not controller-compatible");

			const auto external = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(10),
				selection);
			RevealNavigationClient(
				NavigationPresentationKind::Destinations,
				model,
				external.client,
				presentationState);
			presentation = BuildNavigationPresentation(
				NavigationPresentationKind::Destinations,
				model,
				presentationState);
			const auto staleBridged = std::ranges::find(
				presentation.sourceControls,
				std::string_view{ "Bridged" },
				&NavigationSourceControl::label);
			require(
				external.accepted &&
					external.revealSelection &&
					selection.activePage == 10 &&
					staleBridged != presentation.sourceControls.end() &&
					!staleBridged->selected &&
					ResolveControlledNavigationControlDecision(
						staleBridged->selected,
						false) ==
						ControlledNavigationControlDecision::None,
				"external reveal was interpreted as stale-tab user activation");

			SidebarBrowsingState browsing;
			RevealSidebarSelection(
				SidebarLayoutKind::DrillDown,
				model,
				selection,
				browsing);
			browsing.drillDown = TransitionDrillDown(
				browsing.drillDown,
				DrillDownEvent::Back);
			const auto samePage = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(10),
				selection);
			RevealNavigationClient(
				NavigationPresentationKind::Destinations,
				model,
				samePage.client,
				presentationState);
			RevealSidebarSelection(
				SidebarLayoutKind::DrillDown,
				model,
				selection,
				browsing);
			require(
				samePage.accepted &&
					!samePage.selectionChanged &&
					samePage.revealSelection &&
					browsing.drillDown ==
						DrillDownState{ DrillDownLevel::Pages, 1 },
				"same-page external reveal did not restore layout browsing");
		});

		runner.test("navigation controller applies every selection through one path", [] {
			NavigationModel model;
			model.clients = {
				{
					1,
					"native",
					"Native",
					1,
					{ { {}, {
						{ 10, 1, "first", "First", {}, {}, 0 },
						{ 11, 1, "second", "Second", {}, {}, 10 }
					} } }
				},
				{
					2,
					"bridge",
					"Bridge",
					1,
					{ { {}, {
						{ 20, 2, "page", "Page", {}, {}, 0 }
					} } },
					{},
					DMUI_CLIENT_ORIGIN_BRIDGED,
					"MCM"
				}
			};
			model.sections = {
				{ DMUI_CLIENT_ORIGIN_NATIVE, {}, { 0 } },
				{ DMUI_CLIENT_ORIGIN_BRIDGED, "MCM", { 1 } }
			};
			ClientSelectionState selection;

			const auto client = ApplyNavigationRequest(
				model,
				NavigationRequest::Client(1),
				selection);
			require(
				client.accepted &&
					client.selectionChanged &&
					client.revealSelection &&
					selection.activeClient == 1 &&
					selection.activePage == 10 &&
					!selection.activeHostPage,
				"client request did not select its landing page");

			const auto page = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(11),
				selection);
			const auto samePage = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(11),
				selection);
			require(
				page.accepted &&
					page.selectionChanged &&
					samePage.accepted &&
					!samePage.selectionChanged &&
					samePage.revealSelection &&
					selection.recentPages.front() == 11,
				"same-page request did not retain explicit reveal semantics");

			const auto invalid = ApplyNavigationRequest(
				model,
				NavigationRequest::Page(999),
				selection);
			auto invalidKind = NavigationRequest::Page(11);
			invalidKind.kind =
				static_cast<NavigationRequestKind>(0xFFFFFFFFu);
			const auto unknown = ApplyNavigationRequest(
				model,
				invalidKind,
				selection);
			const auto invalidHost = ApplyNavigationRequest(
				model,
				NavigationRequest::Host(
					static_cast<HostPageKind>(0xFFFFFFFFu)),
				selection);
			const auto invalidClient = ApplyNavigationRequest(
				model,
				NavigationRequest::Client(999),
				selection);
			require(
				!invalid.accepted &&
					!unknown.accepted &&
					!invalidHost.accepted &&
					!invalidClient.accepted &&
					!selection.activeHostPage &&
					selection.activeClient == 1 &&
					selection.activePage == 11,
				"invalid request tag, host, or target changed selection");

			const auto host = ApplyNavigationRequest(
				model,
				NavigationRequest::Host(HostPageKind::kSettings),
				selection);
			require(
				host.accepted &&
					selection.activeHostPage ==
						HostPageKind::kSettings &&
					selection.activeClient == DMUI_INVALID_CLIENT_HANDLE &&
					selection.activePage == DMUI_INVALID_PAGE_HANDLE,
				"host request did not clear client selection");
		});

		runner.test("navigation search exposes pages and actions with row metadata", [] {
			NavigationModel model;
			model.clients.push_back({
				1,
				"dear-modding.addictol",
				"Addictol",
				DMUI_MAKE_VERSION(1, 0),
				{ { "Telemetry", {
					{ 10, 1, "frame-records", "Frame Records", "Telemetry",
						"Inspect captured frame events.", 10, "telemetry-internal" },
					{ 11, 1, "timings", "Timings", "Telemetry",
						"Inspect timing data.", 20, "telemetry-internal" }
				}, "telemetry-internal" } }
			});
			std::vector<RegisteredAction> actions{
				{
					20,
					1,
					"dear-modding.addictol",
					"Addictol",
					"copy-records",
					"Copy Records",
					"clipboard-text",
					"Copy captured frame records.",
					20,
					nullptr,
					nullptr,
					false
				},
				{
					21,
					2,
					"toolbox.mod",
					"Toolbox",
					"open-toolbox",
					"Open Toolbox",
					"toolbox",
					"Open the toolbox.",
					0,
					nullptr,
					nullptr,
					false
				}
			};

			model.BuildSearchIndex(actions);
			require(model.SearchIndex().size() == 5,
				"search index did not include clients, pages, and actions");
			const auto hits = SearchNavigation(model, "records");
			require(hits.size() == 2,
				"page and action did not both match the query");
			require(
					hits[0].Entry().kind == NavigationItemKind::kPage &&
						hits[0].Entry().page == 10 &&
						hits[0].Entry().category == "Telemetry" &&
						hits[1].Entry().kind == NavigationItemKind::kAction &&
						hits[1].Entry().action == 20 &&
						hits[1].Entry().iconName == "clipboard-text" &&
						hits[1].Entry().category.empty(),
					"search hits did not retain actionable row metadata");
			require(
				SearchNavigation(model, "telemetry-internal").empty(),
				"search exposed an opaque category ID");
			const auto actionOnly = SearchNavigation(model, "toolbox");
			require(
					actionOnly.size() == 1 &&
						actionOnly[0].Entry().action == 21 &&
						actionOnly[0].Entry().clientDisplayName == "Toolbox",
					"action-only client was omitted from global search");
			const auto owned = SearchNavigation(model, "ADDICTOL");
			require(
				std::ranges::count_if(
					owned,
					[](const auto& a_hit) {
						return a_hit.Entry().kind ==
							NavigationItemKind::kPage;
					}) == 2,
				"mod-name search did not retain every owned page");
		});

		runner.test("navigation search ranks named matches above summaries case insensitively", [] {
			NavigationModel model;
			model.clients.push_back({
				1,
				"ranking.mod",
				"Ranking",
				DMUI_MAKE_VERSION(1, 0),
				{ { "General", {
					{ 10, 1, "named", "Frame Records", "General", {}, 20 },
					{ 11, 1, "summary", "Diagnostics", "General",
						"Includes frame records and timings.", 0 },
					{ 12, 1, "zulu", "Zulu", "General", "shared token", 10 },
					{ 13, 1, "bravo", "Bravo", "General", "shared token", -10 },
					{ 14, 1, "alpha", "Alpha", "General", "shared token", 10 }
				} } }
			});

			model.BuildSearchIndex({});
			const auto hits = SearchNavigation(model, "fRaMe ReCoRdS");
			require(hits.size() == 2,
				"case-insensitive search lost a matching page");
			require(
					hits[0].Entry().page == 10 &&
						hits[0].match ==
							NavigationMatchQuality::kDisplayNameExact &&
						hits[1].Entry().page == 11 &&
						hits[1].match == NavigationMatchQuality::kSummary,
					"title match did not outrank a summary match");
			const auto ties = SearchNavigation(model, "token");
			require(ties.size() == 3,
				"equal-quality search did not return every hit");
			require(
					ties[0].Entry().id == "bravo" &&
						ties[1].Entry().id == "alpha" &&
						ties[2].Entry().id == "zulu",
					"equal-quality hits ignored sort key or stable ID");
		});

		runner.test("recent pages stay bounded unique and reject unknown handles", [] {
			NavigationModel model;
			model.clients.push_back({
				1,
				"recent.mod",
				"Recent",
				DMUI_MAKE_VERSION(1, 0),
				{ { "General", {
					{ 10, 1, "one", "One", "General", {}, 0 },
					{ 11, 1, "two", "Two", "General", {}, 10 },
					{ 12, 1, "three", "Three", "General", {}, 20 }
				} } }
			});
			ClientSelectionState state;
			RecordRecentPage(model, 10, state, 2);
			RecordRecentPage(model, 11, state, 2);
			RecordRecentPage(model, 10, state, 2);
			require(state.recentPages == std::vector<DMUI_PageHandle>{ 10, 11 },
				"recent pages did not move duplicates to the front");
			RecordRecentPage(model, 12, state, 2);
			require(state.recentPages == std::vector<DMUI_PageHandle>{ 12, 10 },
				"recent pages did not evict the oldest handle");
			RecordRecentPage(model, 9999, state, 2);
			require(state.recentPages == std::vector<DMUI_PageHandle>{ 12, 10 },
				"unknown page entered the recent list");
		});

		runner.test("page row labels namespace duplicate page IDs by mod", [] {
			const NavigationPage firstPage{
				10, 1, "settings", "Settings", "General", {}, 0
			};
			const NavigationPage secondPage{
				20, 2, "settings", "Settings", "General", {}, 0
			};
			const NavigationClient firstClient{
				1, "first.mod", "First", DMUI_MAKE_VERSION(1, 0), {}
			};
			const NavigationClient secondClient{
				2, "second.mod", "Second", DMUI_MAKE_VERSION(1, 0), {}
			};

			const auto firstLabel = PageRowLabel(firstClient, firstPage);
			const auto secondLabel = PageRowLabel(secondClient, secondPage);
			require(firstLabel == "###DearModdingPage/first.mod/settings",
				"page row label retained visible padding or changed ID format");
			require(secondLabel == "###DearModdingPage/second.mod/settings",
				"page row label omitted the owning mod ID");
			require(firstLabel != secondLabel,
				"duplicate page IDs in different mods produced colliding row labels");
		});

	}
}
