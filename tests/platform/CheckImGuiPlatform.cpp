#include <Platform/rendering/ImGuiPlatformTargets.h>
#include <Platform/rendering/FrameSubmission.h>
#include <Platform/input/GameInput.h>
#include <Platform/input/ToggleOutcome.h>
#include "../Harness.h"

namespace
{
	using namespace DearModdingUI::GameInput;
	using namespace DearModdingUI::PlatformImGui;

	static_assert(kPresentSlot == 8, "Present must use DXGI vtable slot 8");
	static_assert(kResizeBuffersSlot == 13, "ResizeBuffers must use DXGI vtable slot 13");
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
		runner.test("native cursor and Present share one submission per active frame", [] {
			FrameSubmission frame;
			constexpr PresentAttachmentToken first{ 11, 7 };
			constexpr PresentAttachmentToken rebound{ 11, 8 };
			require(!frame.Claim({}) && frame.Claim(first),
				"invalid attachment claimed a frame or valid attachment was rejected");
			require(!frame.Claim(first) && !frame.Submitted(first),
				"reentrant or repeated cursor predicate entered a second frame");
			frame.Complete(first);
			const auto firstSequence = frame.Sequence();
			require(frame.Submitted(first) && !frame.Claim(first),
				"Present could draw again after the native draw closed the modal");
			frame.FinishPresent(first, firstSequence, kPresentTestFlag, true);
			frame.FinishPresent(first, firstSequence, 0, false);
			frame.FinishPresent({ 12, 7 }, firstSequence, 0, true);
			require(frame.Submitted(first) && !frame.Claim(first),
				"test, failed, or unrelated Present released the active frame");
			frame.FinishPresent(first, firstSequence, 0, true);
			require(frame.Claim(first),
				"a successful Present did not release the next frame");
			require(frame.Claim(rebound),
				"new attachment generation inherited an old submission");
			frame.Complete(first);
			frame.FinishPresent(first, firstSequence, 0, true);
			require(!frame.Submitted(rebound) && !frame.Claim(rebound),
				"stale draw or Present changed the new attachment's frame");
			frame.Complete(rebound);
			frame.FinishPresent(rebound, frame.Sequence(), 0, true);
			require(frame.Claim(rebound), "real Present did not release the next frame");
			frame.Reset();
			require(frame.Claim(rebound), "renderer retirement did not discard its submission");

			constexpr PresentAttachmentToken attachment{ 11, 7 };
			require(frame.Claim(attachment), "initial frame claim failed");
			frame.Complete(attachment);
			const auto delayedPresent = frame.Sequence();
			frame.FinishPresent(attachment, delayedPresent, 0, true);
			require(frame.Claim(attachment), "next native frame claim failed");
			frame.Complete(attachment);
			const auto currentPresent = frame.Sequence();
			frame.FinishPresent(attachment, delayedPresent, 0, true);
			require(frame.Submitted(attachment) && !frame.Claim(attachment),
				"an older nested or deferred Present released a newer native frame");
			frame.FinishPresent(attachment, currentPresent, 0, true);
			require(frame.Claim(attachment), "current Present did not release its frame");
			frame.Reset();
			require(frame.Claim(attachment), "post-resize frame claim failed");
			frame.Complete(attachment);
			frame.FinishPresent(attachment, currentPresent, 0, true);
			require(frame.Submitted(attachment),
				"a pre-resize Present released a post-resize submission");
		});

		runner.test("renderer attachment lifecycle and result classes stay coherent", [] {
			constexpr AttachmentIdentity empty{};
			constexpr AttachmentIdentity game{ 1, 2, 3, 4 };
			constexpr AttachmentIdentity reboundGame{ 5, 6, 7, 8 };
			constexpr AttachmentIdentity explicitOverride{ 5, 2, 3, 4 };
			constexpr AttachmentIdentity nextGeneration{ 6, 7, 8, 4 };
			constexpr RendererProbe renderer{ true, true, true, game };

			require(ObserveRenderer(renderer) == RendererObservation::kReady,
				"a complete renderer binding must be usable");
			require(
				DecideAttachment(
					empty,
					game,
					AttachmentSource::kRenderer,
					AttachmentSource::kRenderer,
					AttachmentLifecycle::kVacant) ==
					AttachmentDecision::kAttach,
				"the first renderer binding must attach");
			require(
				DecideAttachment(
					{ 1, 2, 3, 4 }, {}, AttachmentSource::kRenderer,
					AttachmentSource::kExplicit,
					AttachmentLifecycle::kActive) ==
					AttachmentDecision::kReject,
				"invalid candidate validation was weakened");
			require(
				DecideAttachment(
					game,
					game,
					AttachmentSource::kRenderer,
					AttachmentSource::kRenderer,
					AttachmentLifecycle::kActive) ==
					AttachmentDecision::kKeepCurrent,
				"an unchanged binding must return before hook installation");
			require(
				DecideAttachment(
					game,
					reboundGame,
					AttachmentSource::kRenderer,
					AttachmentSource::kRenderer,
					AttachmentLifecycle::kActive) ==
					AttachmentDecision::kReplace,
				"a changed renderer generation must retire and replace the active binding");
			require(
				DecideAttachment(
					game,
					explicitOverride,
					AttachmentSource::kRenderer,
					AttachmentSource::kExplicit,
					AttachmentLifecycle::kActive) ==
					AttachmentDecision::kReplace,
				"an explicit override must replace the renderer swapchain");
			require(
				DecideAttachment(
					explicitOverride,
					game,
					AttachmentSource::kExplicit,
					AttachmentSource::kRenderer,
					AttachmentLifecycle::kActive) ==
					AttachmentDecision::kKeepCurrent,
				"reconciliation must not undo an override in the same renderer generation");
			require(
				DecideAttachment(
					explicitOverride,
					nextGeneration,
					AttachmentSource::kExplicit,
					AttachmentSource::kRenderer,
					AttachmentLifecycle::kActive) ==
					AttachmentDecision::kReplace,
				"a renderer generation change must retire a stale override");
		});

		runner.test("swapchain dispatch survives shadow vtable retargeting", [] {
			require(
				MatchHookDispatch(1, 20, 1, 10) == HookDispatchMatch::kSwapChain,
				"an associated swapchain must keep its captured predecessor after its vptr changes");
			require(
				MatchHookDispatch(2, 10, 1, 10) == HookDispatchMatch::kVtable,
				"an unassociated instance on the patched vtable must use the vtable predecessor");
			require(
				MatchHookDispatch(2, 20, 1, 10) == HookDispatchMatch::kNone,
				"an unrelated instance and vtable must not borrow another predecessor");
			require(
				ReusesHookAssociation(AttachmentLifecycle::kActive, 20, 10),
				"an active proxy must retain its predecessor after shadow-vtable retargeting");
			require(
				ReusesHookAssociation(AttachmentLifecycle::kRetired, 10, 10),
				"a reused address on the same patched vtable must retain its predecessor");
			require(
				!ReusesHookAssociation(AttachmentLifecycle::kRetired, 20, 10),
				"a reused address with a new vtable must establish a new predecessor");
		});

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
			require(ClassifyReadiness(false, true, false) == Readiness::kRendererNotAttached &&
					ClassifyReadiness(true, false, true) == Readiness::kBackendFailed &&
					ClassifyReadiness(true, false, false) == Readiness::kBackendUninitialized &&
					ClassifyReadiness(true, true, false) == Readiness::kReady,
				"renderer readiness misnamed its gate");

			Gates gates{ true, true, Readiness::kBackendUninitialized, false };
			require(Classify(gates) == Outcome::kBackendUninitialized, "a pre-Present press was not named");
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
