#include "../support/DearModdingUITestSupport.h"
#include <Platform/input/CarrierMenu.h>
#include <DearModdingUI/host/MenuToggleKey.h>
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
		runner.test("cursor ownership follows modal visibility", [] {
			const auto overlay = DecideCursorPresentation(false);
			require(!overlay.captureInput &&
					!overlay.hideOperatingSystemCursor &&
					!overlay.drawSoftwareCursor &&
					!overlay.drawCustomCursor,
				"overlay-only drawing acquired a cursor");

			const auto modal = DecideCursorPresentation(true);
			require(modal.captureInput &&
					modal.hideOperatingSystemCursor &&
					modal.drawSoftwareCursor &&
					!modal.drawCustomCursor,
				"modal drawing did not own exactly one software cursor");
			require(
				static_cast<uint32_t>(modal.drawSoftwareCursor) +
						static_cast<uint32_t>(modal.drawCustomCursor) ==
					1,
				"the modal host did not present exactly one cursor");

			require(DecideCursorTransition(false, true) ==
					CursorOwnershipTransition::kAcquire,
				"menu open did not acquire cursor ownership");
			require(DecideCursorTransition(true, false) ==
					CursorOwnershipTransition::kRelease,
				"menu close did not release cursor ownership");
			require(DecideCursorTransition(true, true) ==
					CursorOwnershipTransition::kNone,
				"steady modal state retriggered ownership");
			require(DecideCursorTransition(false, false) ==
					CursorOwnershipTransition::kNone,
				"steady overlay state changed ownership");
		});

		runner.test("carrier menu open and close messages remain balanced", [] {
			CarrierMenu::State state{};
			require(
				CarrierMenu::Transition(state, CarrierMenu::Event::kOpen) ==
					CarrierMenu::Action::kShow,
				"the first modal open did not show the carrier");
			require(
				CarrierMenu::Transition(state, CarrierMenu::Event::kOpen) ==
					CarrierMenu::Action::kNone,
				"a repeated modal frame showed the carrier twice");
			require(
				CarrierMenu::Transition(state, CarrierMenu::Event::kClose) ==
					CarrierMenu::Action::kHide,
				"the modal close did not hide the carrier");
			require(
				CarrierMenu::Transition(state, CarrierMenu::Event::kClose) ==
					CarrierMenu::Action::kNone,
				"a repeated close hid the carrier twice");
			require(!state.open, "the balanced sequence retained cursor ownership");
		});

		runner.test("carrier menu cleanup paths hide exactly one open entry", [] {
			constexpr std::array cleanupEvents{
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
