#include <Platform/rendering/ImGuiPlatformTargets.h>
#include <Platform/rendering/FrameSubmission.h>
#include <Platform/input/GameInput.h>
#include "../Harness.h"

namespace
{
	using namespace Addictol::GameInput;
	using namespace Addictol::ImguiPlatform;

	static_assert(kPresentSlot == 8, "Present must use DXGI vtable slot 8");
	static_assert(kResizeBuffersSlot == 13, "ResizeBuffers must use DXGI vtable slot 13");
}

namespace vmm_tests
{
	void run_gamepad_input_checks(Runner& runner);

	void run_imgui_platform_checks(Runner& runner)
	{
		runner.test("Win32 scan codes distinguish extended keys and lock overrides", [] {
			require(KeyboardKeyCode(0x1D, false, 0x11) == 0x1D &&
					KeyboardKeyCode(0x1D, true, 0x11) == 0x9D,
				"left and right Ctrl collided");
			require(KeyboardKeyCode(0x48, false, 0x68) == 0x48 &&
					KeyboardKeyCode(0x48, true, 0x26) == 0xC8 &&
					KeyboardKeyCode(0x1C, true, 0x0D) == 0x9C,
				"numpad and extended navigation keys collided");
			require(KeyboardKeyCode(0x45, false, 0x13) == 0xC5 &&
					KeyboardKeyCode(0x45, true, 0x90) == 0x45,
				"Pause and NumLock overrides did not preserve identity");
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

		runner.test("window messages are classified and swallowed by capture state", [] {
			require(ClassifyMessage(0x0100) == MessageClass::kKeyboard, "WM_KEYDOWN is keyboard");
			require(ClassifyMessage(0x0200) == MessageClass::kMouse, "WM_MOUSEMOVE is mouse");
			require(ClassifyMessage(0x00FF) == MessageClass::kOther, "WM_INPUT is neither");

			require(SwallowsGameWindowMessage(0x0201, true, false),
				"captured mouse input did not stop at the menu");
			require(SwallowsGameWindowMessage(0x0100, false, true),
				"captured keys did not stop at the menu");
			require(!SwallowsGameWindowMessage(0x0201, false, false) &&
					!SwallowsGameWindowMessage(0x00FF, true, true),
				"uncaptured or non-input messages stopped reaching the game");
			require(!SwallowsGameWindowMessage(0x0200, true, true),
				"WM_MOUSEMOVE cannot reach the game's native cursor");
		});

		runner.test("Escape ownership follows the visible host press pair", [] {
			require(
				DecideEscapeMessage(
					kKeyDownMessage,
					kEscapeVirtualKey,
					1,
					false,
					false) == EscapeMessageDecision::kForward,
				"closed-host Escape must remain game-owned");
			require(
				DecideEscapeMessage(
					kKeyDownMessage,
					kEscapeVirtualKey,
					1,
					true,
					false) == EscapeMessageDecision::kCapture,
				"a fresh visible-host Escape must be captured");
			require(
				DecideEscapeMessage(
					kKeyDownMessage,
					kEscapeVirtualKey,
					kKeyRepeatBit | 1,
					false,
					true) == EscapeMessageDecision::kConsume,
				"a held captured Escape leaked after the host closed");
			require(
				DecideEscapeMessage(
					kKeyUpMessage,
					kEscapeVirtualKey,
					1,
					false,
					true) ==
					EscapeMessageDecision::kConsumeAndRelease,
				"the captured Escape release leaked after host close");
			require(
				DecideEscapeMessage(
					kKeyDownMessage,
					kEscapeVirtualKey,
					kKeyRepeatBit | 1,
					true,
					false) == EscapeMessageDecision::kForward,
				"opening the host while Escape is held captured a repeat");
			require(
				DecideEscapeMessage(
					kKeyDownMessage,
					kEscapeVirtualKey,
					1,
					false,
					true) ==
					EscapeMessageDecision::kReleaseAndForward,
				"a new closed-host Escape inherited stale ownership");
			require(
				DecideEscapeMessage(
					kKeyUpMessage,
					0x10,
					1,
					true,
					true) == EscapeMessageDecision::kForward,
				"captured Escape also consumed a modifier release");
			require(
				DecideEscapeMessage(
					0x0008,
					kEscapeVirtualKey,
					0,
					false,
					true) == EscapeMessageDecision::kForward &&
				DecideEscapeMessage(
					kKeyDownMessage,
					kEscapeVirtualKey,
					kKeyRepeatBit | 1,
					false,
					true) == EscapeMessageDecision::kConsume,
				"focus loss discarded ownership before a held repeat");
		});
	}
}
