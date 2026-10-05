#include "../support/DearModdingUITestSupport.h"
#include <Platform/input/CarrierMenu.h>
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

	void run_carrier_menu_checks(Runner& runner)
	{
		runner.test("carrier menu cleanup paths hide exactly one open entry", [] {
			constexpr std::array cleanupEvents{
				CarrierMenu::Event::kClose,
				CarrierMenu::Event::kShutdown,
				CarrierMenu::Event::kBackendFailure,
				CarrierMenu::Event::kRetarget,
				CarrierMenu::Event::kGameTransition,
				CarrierMenu::Event::kOverlayOnly
			};
			for (const auto event : cleanupEvents)
			{
				CarrierMenu::State state{};
				require(
					CarrierMenu::Transition(state, CarrierMenu::Event::kOpen) ==
						CarrierMenu::Action::kShow,
					"a cleanup scenario did not establish an open carrier");
				require(
					CarrierMenu::Transition(state, CarrierMenu::Event::kOpen) ==
						CarrierMenu::Action::kNone,
					"a repeated modal frame showed the carrier twice");
				require(
					CarrierMenu::Transition(state, event) ==
						CarrierMenu::Action::kHide,
					"a cleanup scenario did not balance its show");
				require(
					CarrierMenu::Transition(state, event) ==
						CarrierMenu::Action::kNone,
					"a cleanup scenario queued a second hide");
				require(!state.open, "a cleanup scenario retained cursor ownership");
			}

			CarrierMenu::State overlay{};
			require(
				CarrierMenu::Transition(
					overlay,
					CarrierMenu::Event::kOverlayOnly) ==
					CarrierMenu::Action::kNone,
				"an overlay-only frame opened or hid a carrier");
		});
	}
}
