#include "../support/DearModdingUITestSupport.h"
#include <Platform/menus/HostMenu.h>
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

	void run_host_menu_checks(Runner& runner)
	{
		runner.test("host menu cleanup paths hide exactly one open entry", [] {
			constexpr std::array cleanupEvents{
				HostMenu::Event::kClose,
				HostMenu::Event::kShutdown,
				HostMenu::Event::kBackendFailure,
				HostMenu::Event::kRetarget,
				HostMenu::Event::kGameTransition
			};
			for (const auto event : cleanupEvents)
			{
				HostMenu::State state{};
				require(
					HostMenu::Transition(state, HostMenu::Event::kOpen) ==
						HostMenu::Action::kShow,
					"a cleanup scenario did not establish an open host menu");
				require(
					HostMenu::Transition(state, HostMenu::Event::kOpen) ==
						HostMenu::Action::kNone,
					"a repeated modal frame showed the host menu twice");
				require(
					HostMenu::Transition(state, event) ==
						HostMenu::Action::kHide,
					"a cleanup scenario did not balance its show");
				require(
					HostMenu::Transition(state, event) ==
						HostMenu::Action::kNone,
					"a cleanup scenario queued a second hide");
				require(!state.open, "a cleanup scenario retained cursor ownership");
			}
		});
	}
}
