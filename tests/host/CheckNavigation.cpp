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

			const auto recentBefore = selection.recentPages;
			RecordRecentPage(model, 9999, selection);
			require(selection.recentPages == recentBefore,
				"unknown page entered the recent list");
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
			require(firstLabel != secondLabel,
				"duplicate page IDs in different mods produced colliding row labels");
		});
	}
}
