#include <Platform/rendering/ImGuiPlatformTargets.h>
#include <Platform/input/GameInput.h>
#include <Platform/input/ToggleOutcome.h>
#include "../Harness.h"

namespace
{
	using namespace DearModdingUI::GameInput;
	using namespace DearModdingUI::PlatformImGui;
}

namespace vmm_tests
{
	void run_gamepad_input_checks(Runner& runner);

	void run_imgui_platform_checks(Runner& runner)
	{
		runner.test("engine mouse ids map to catalog button and wheel codes", [] {
			using namespace DearModdingUI::KeyCatalog;
			for (uint32_t id = 0; id < kMouseButtonCount; ++id)
				require(MouseKeyCode(id) == kMouseButtonOffset + id, "mouse button code drifted");
			require(MouseKeyCode(0x800) == kMouseWheelOffset && MouseKeyCode(0x900) == kMouseWheelOffset + 1 &&
					MouseKeyCode(kMouseButtonCount) == 0 && MouseKeyCode(0x1000) == 0,
				"wheel or unknown mouse id mapped incorrectly");
		});
		run_gamepad_input_checks(runner);
		runner.test("Escape ownership follows the visible host press pair", [] {
			require(
				DecideEscapeEdge(true, false, false, false) == EscapeDecision::kForward,
				"closed-host Escape must remain game-owned");
			require(
				DecideEscapeEdge(true, false, true, false) == EscapeDecision::kCapture,
				"a fresh visible-host Escape must be captured");
			require(
				DecideEscapeEdge(true, true, false, true) == EscapeDecision::kConsume,
				"a held captured Escape leaked after the host closed");
			require(
				DecideEscapeEdge(false, false, false, true) == EscapeDecision::kConsumeAndRelease,
				"the captured Escape release leaked after host close");
			require(
				DecideEscapeEdge(true, true, true, false) == EscapeDecision::kForward,
				"opening the host while Escape is held captured a repeat");
			require(
				DecideEscapeEdge(true, false, false, true) == EscapeDecision::kReleaseAndForward,
				"a new closed-host Escape inherited stale ownership");
			require(
				DecideEscapeEdge(false, false, true, false) == EscapeDecision::kForward,
				"an unowned Escape release was consumed");
		});

		runner.test("a refused toggle names the first gate that stopped it", [] {
			using namespace DearModdingUI::ToggleOutcome;
			Gates gates{ true, true, Readiness::kBackendUninitialized, false };
			require(Classify(gates) == Outcome::kBackendUninitialized, "a press while game data loads was not named");
			gates.engineActive = false;
			require(Classify(gates) == Outcome::kEngineInactive, "engine focus loss was hidden by the renderer gate");
			gates.inputHooks = false;
			require(Classify(gates) == Outcome::kInputHooksFailed, "failed hooks were hidden by a later gate");
			gates = { true, true, Readiness::kReady, false };
			require(Classify(gates) == Outcome::kHostNotReady, "a refusal with every gate open was unnamed");
			gates.hostAccepted = true;
			require(Classify(gates) == Outcome::kToggled, "an accepted toggle was reported as dropped");

			ChangeFilter filter;
			require(filter.Changed(Outcome::kBackendFailed) && !filter.Changed(Outcome::kBackendFailed) &&
					filter.Changed(Outcome::kToggled),
				"repeated presses with one cause must log once");
		});
	}
}
