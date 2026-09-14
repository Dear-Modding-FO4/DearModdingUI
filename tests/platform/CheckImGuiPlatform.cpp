#include <Platform/rendering/ImGuiPlatformTargets.h>
#include <Platform/rendering/FrameSubmission.h>
#include <Platform/input/GameInput.h>
#include <DearModdingUI/host/SwapChainAttachment.h>
#include "../Harness.h"

#include <limits>
#include <string>

namespace
{
	using namespace Addictol::GameInput;
	using namespace Addictol::ImguiPlatform;

	static_assert(kPresentSlot == 8, "Present must use DXGI vtable slot 8");
	static_assert(kResizeBuffersSlot == 13, "ResizeBuffers must use DXGI vtable slot 13");
}

namespace vmm_tests
{
	void run_cursor_ownership_checks(Runner& runner);

	void run_imgui_platform_checks(Runner& runner)
	{
		run_cursor_ownership_checks(runner);
		runner.test("native cursor and Present share one submission per active frame", [] {
			FrameSubmission frame;
			constexpr PresentAttachmentToken first{ 11, 7 };
			constexpr PresentAttachmentToken rebound{ 11, 8 };
			require(!frame.Claim({}) && frame.Claim(first),
				"invalid attachment claimed a frame or valid attachment was rejected");
			require(!frame.Claim(first) && !frame.Submitted(first),
				"reentrant or repeated cursor predicate entered a second frame");
			frame.Complete(first);
			require(frame.Submitted(first) && !frame.Claim(first),
				"Present could draw again after the native draw closed the modal");
			frame.FinishPresent(first, kPresentTestFlag, true);
			frame.FinishPresent(first, 0, false);
			frame.FinishPresent({ 12, 7 }, 0, true);
			require(frame.Submitted(first) && !frame.Claim(first),
				"test, failed, or unrelated Present released the active frame");
			frame.FinishPresent(first, 0, true);
			require(frame.Claim(first),
				"a successful Present did not release the next frame");
			require(frame.Claim(rebound),
				"new attachment generation inherited an old submission");
			frame.Complete(first);
			frame.FinishPresent(first, 0, true);
			require(!frame.Submitted(rebound) && !frame.Claim(rebound),
				"stale draw or Present changed the new attachment's frame");
			frame.Complete(rebound);
			frame.FinishPresent(rebound, 0, true);
			require(frame.Claim(rebound), "real Present did not release the next frame");
			frame.Reset();
			require(frame.Claim(rebound), "renderer retirement did not discard its submission");
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
				FailedAttachmentResult(ObserveRenderer({})) ==
						AttachmentResult::kNotReady &&
					DearModdingUI::SwapChainAttachmentResult(
						AttachmentResult::kNotReady) ==
						DMUI_RESULT_HOST_NOT_READY,
				"an incomplete renderer was not classified as retryable startup");
			require(
				DearModdingUI::SwapChainAttachmentResult(
					FailedAttachmentResult(RendererObservation::kBindingChanged)) ==
						DMUI_RESULT_RENDERER_BUSY &&
					DearModdingUI::SwapChainAttachmentResult(
						FailedAttachmentResult(
							RendererObservation::kInvalidBinding)) ==
						DMUI_RESULT_SWAPCHAIN_REJECTED &&
					DearModdingUI::SwapChainAttachmentResult(
						AttachmentResult::kAttached) == DMUI_RESULT_OK,
				"attachment result classes lost retryable, permanent, or success mapping");
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

		runner.test("definitive DXGI failures retire the active attachment", [] {
			require(IsDefinitiveSwapChainLoss(kDxgiErrorDeviceRemoved),
				"device removal must retire the attachment");
			require(IsDefinitiveSwapChainLoss(kDxgiErrorDeviceHung),
				"a device hang must retire the attachment");
			require(IsDefinitiveSwapChainLoss(kDxgiErrorDeviceReset),
				"a device reset must retire the attachment");
			require(IsDefinitiveSwapChainLoss(kDxgiErrorDriverInternal),
				"an internal driver failure must retire the attachment");
			require(!IsDefinitiveSwapChainLoss(0), "success must keep the attachment");
			require(!IsDefinitiveSwapChainLoss(0x887A0001u),
				"a transient invalid call must keep the attachment");
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

		runner.test("frame telemetry observes only displayed presents", [] {
			require(ObservesDisplayedFrame(0, true), "a successful real Present displays a frame");
			require(!ObservesDisplayedFrame(kPresentTestFlag, true), "DXGI_PRESENT_TEST displays no frame");
			require(!ObservesDisplayedFrame(0, false), "a failed Present displays no frame");
		});

		runner.test("post-Present observers require the captured active attachment", [] {
			constexpr PresentAttachmentToken presented{ 11, 7 };
			require(MatchesActivePresentAttachment(
						presented, 11, 7, AttachmentLifecycle::kActive),
				"current displayed Present token was rejected");
			require(!MatchesActivePresentAttachment(
						presented, 12, 7, AttachmentLifecycle::kActive),
				"replaced swapchain retained stale observer dispatch");
			require(!MatchesActivePresentAttachment(
						presented, 11, 8, AttachmentLifecycle::kActive),
				"same-address attachment rebind retained stale observer dispatch");
			require(!MatchesActivePresentAttachment(
						presented, 11, 7, AttachmentLifecycle::kRetired),
				"retired attachment retained observer dispatch");
			require(!MatchesActivePresentAttachment(
						{}, 11, 7, AttachmentLifecycle::kActive),
				"uncaptured Present acquired observer dispatch");
		});

		runner.test("input hook health retains the concrete failed receiver", [] {
			auto outcomes = std::array{
				InputReceiverHookOutcome{
					InputReceiver::kMenuControls,
					InputHookFailure::kNone },
				InputReceiverHookOutcome{
					InputReceiver::kPlayerControls,
					InputHookFailure::kPatchFailed },
				InputReceiverHookOutcome{
					InputReceiver::kPlayerCamera,
					InputHookFailure::kNone }
			};
			auto observation = ClassifyInputHookHealth(outcomes);
			require(
				observation.state == DearModdingUI::HealthState::kFailed &&
					observation.reason.find("PlayerControls") !=
						std::string::npos &&
					observation.reason.find("incompatible input hook") !=
						std::string::npos,
				"input health lost the receiver patch failure");

			outcomes[1].failure = InputHookFailure::kNone;
			outcomes[2].failure = InputHookFailure::kOriginalTargetLost;
			observation = ClassifyInputHookHealth(outcomes);
			require(
				observation.state == DearModdingUI::HealthState::kFailed &&
					observation.reason.find("PlayerCamera") !=
						std::string::npos &&
					observation.reason.find("original target") !=
						std::string::npos,
				"runtime original-target loss was not actionable");
		});

		runner.test("backbuffer state recreates on identity size and view changes", [] {
			constexpr BackBufferIdentity first{ 1, 1920, 1080 };
			constexpr BackBufferIdentity replacement{ 2, 1920, 1080 };
			constexpr BackBufferIdentity resized{ 1, 2560, 1440 };
			constexpr BackBufferIdentity invalid{ 1, 0, 1080 };

			require(
				DecideBackBuffer(first, first, true) == BackBufferDecision::kKeep,
				"an unchanged backbuffer must keep its RTV");
			require(
				DecideBackBuffer(first, first, false) == BackBufferDecision::kRecreate,
				"a missing RTV must be recreated");
			require(
				DecideBackBuffer(first, replacement, true) == BackBufferDecision::kRecreate,
				"a replacement resource must recreate its RTV");
			require(
				DecideBackBuffer(first, resized, true) == BackBufferDecision::kRecreate,
				"a size change must recreate the RTV");
			require(
				DecideBackBuffer(first, invalid, true) == BackBufferDecision::kSkip,
				"an invalid backbuffer must skip rendering");
		});

		runner.test("mouse coordinates map from the client into the backbuffer", [] {
			constexpr auto nonUniform =
				MapClientToBackBuffer({ 400.0f, 300.0f }, 800, 600, 2560, 1080);
			require(nonUniform.x == 1280.0f && nonUniform.y == 540.0f,
				"independent axis scaling changed");

			constexpr MousePosition position{ 400.0f, 300.0f };
			constexpr auto zeroClientWidth =
				MapClientToBackBuffer(position, 0, 600, 2560, 1080);
			require(
				zeroClientWidth.x == position.x &&
					zeroClientWidth.y == position.y,
				"degenerate dimensions changed mouse coordinates");

			constexpr auto unavailable =
				-(std::numeric_limits<float>::max)();
			constexpr auto sentinel =
				MapClientToBackBuffer({ unavailable, unavailable }, 800, 600, 2560, 1080);
			require(sentinel.x == unavailable && sentinel.y == unavailable,
				"the unavailable mouse sentinel was scaled");

			constexpr MousePosition belowOutside{ 400.0f, 601.0f };
			constexpr auto belowMapped =
				MapClientToBackBuffer(belowOutside, 800, 600, 2560, 1080);
			require(
				belowMapped.x == belowOutside.x &&
					belowMapped.y == belowOutside.y,
				"out-of-window mouse coordinates were scaled into the viewport");
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

		runner.test("toggle callback fires once per physical press", [] {
			require(
				DecideToggleMessage(kKeyDownMessage, 1, false) ==
					ToggleMessageDecision::kDispatch,
				"a fresh keydown must invoke the toggle callback");
			require(
				DecideToggleMessage(kSysKeyDownMessage, 1, false) ==
					ToggleMessageDecision::kDispatch,
				"a fresh system keydown must invoke the toggle callback");
			require(
				DecideToggleMessage(kKeyDownMessage, kKeyRepeatBit | 1, true) ==
					ToggleMessageDecision::kConsume,
				"a consumed press must also consume repeats");
			require(
				DecideToggleMessage(kKeyDownMessage, kKeyRepeatBit | 1, false) ==
					ToggleMessageDecision::kForward,
				"an unrelated repeat must reach the game");
			require(
				DecideToggleMessage(kKeyUpMessage, 1, true) ==
					ToggleMessageDecision::kConsumeAndRelease,
				"a consumed press must consume and release its key-up");
			require(
				DecideToggleMessage(kSysKeyUpMessage, 1, true) ==
					ToggleMessageDecision::kConsumeAndRelease,
				"a consumed system press must consume and release its key-up");
			require(
				DecideToggleMessage(kKeyUpMessage, 1, false) ==
					ToggleMessageDecision::kForward,
				"an unrelated key-up must reach the game");
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
