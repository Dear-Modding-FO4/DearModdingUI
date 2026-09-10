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

	void run_page_activity_checks(Runner& runner)
	{
		runner.test("page activity reports client boundaries without false closes", [] {
			Registry registry;
			CallbackState firstState;
			CallbackState secondState;
			const auto firstClient = AddClient(
				registry,
				"first.mod",
				"First",
				firstState);
			const auto secondClient = AddClient(
				registry,
				"second.mod",
				"Second",
				secondState);
			AddCategory(registry, firstClient, "general", "General");
			AddCategory(registry, secondClient, "general", "General");
			const auto firstPage = AddPage(
				registry,
				firstClient,
				"first",
				"First",
				"general",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				firstState);
			const auto nextPage = AddPage(
				registry,
				firstClient,
				"next",
				"Next",
				"general",
				1,
				DMUI_PAGE_KIND_SETTINGS,
				firstState);
			const auto secondPage = AddPage(
				registry,
				secondClient,
				"second",
				"Second",
				"general",
				0,
				DMUI_PAGE_KIND_SETTINGS,
				secondState);
			PageActivityState firstActivity;
			PageActivityState secondActivity;
			const DMUI_PageActivityObserverDescriptor firstObserver{
				sizeof(DMUI_PageActivityObserverDescriptor),
				&ObservePageActivity,
				&firstActivity
			};
			const DMUI_PageActivityObserverDescriptor secondObserver{
				sizeof(DMUI_PageActivityObserverDescriptor),
				&ObservePageActivity,
				&secondActivity
			};
			DMUI_PageActivityObserverHandle observer{};
			require(
				registry.RegisterPageActivityObserver(
					firstClient,
					&firstObserver,
					&observer) == DMUI_RESULT_OK &&
					registry.RegisterPageActivityObserver(
						secondClient,
						&secondObserver,
						&observer) == DMUI_RESULT_OK &&
					registry.Freeze(),
				"page activity observers did not register");

			registry.NotifyPageActivity(DMUI_INVALID_PAGE_HANDLE, firstPage);
			registry.NotifyPageActivity(firstPage, nextPage);
			registry.NotifyPageActivity(nextPage, secondPage);
			registry.NotifyPageActivity(secondPage, DMUI_INVALID_PAGE_HANDLE);

			require(
				firstActivity.events.size() == 3 &&
					firstActivity.events[0].kind ==
						DMUI_PAGE_ACTIVITY_ACTIVATED &&
					firstActivity.events[0].activePage == firstPage &&
					firstActivity.events[1].kind ==
						DMUI_PAGE_ACTIVITY_CHANGED &&
					firstActivity.events[1].previousPage == firstPage &&
					firstActivity.events[1].activePage == nextPage &&
					firstActivity.events[2].kind ==
						DMUI_PAGE_ACTIVITY_DEACTIVATED &&
					firstActivity.events[2].previousPage == nextPage &&
					secondActivity.events.size() == 2 &&
					secondActivity.events[0].kind ==
						DMUI_PAGE_ACTIVITY_ACTIVATED &&
					secondActivity.events[0].activePage == secondPage &&
					secondActivity.events[1].kind ==
						DMUI_PAGE_ACTIVITY_DEACTIVATED,
				"page activity invented a close/open pair within one client");
		});

	}
}
