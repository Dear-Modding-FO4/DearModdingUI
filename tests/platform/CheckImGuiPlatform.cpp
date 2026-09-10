#include <Platform/rendering/ImGuiPlatformTargets.h>
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
	void run_imgui_platform_checks(Runner& runner)
	{
		runner.test("renderer attachment lifecycle and result mapping stay coherent", [] {
			constexpr AttachmentIdentity empty{};
			constexpr AttachmentIdentity game{ 1, 2, 3, 4 };
			constexpr AttachmentIdentity reboundGame{ 5, 6, 7, 8 };
			constexpr AttachmentIdentity explicitOverride{ 5, 2, 3, 4 };
			constexpr AttachmentIdentity nextGeneration{ 6, 7, 8, 4 };
			constexpr RendererProbe renderer{ true, true, true, game };

			require(
				ObserveRenderer(renderer) == RendererObservation::kReady,
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
			constexpr std::array probes{
				RendererProbe{},
				RendererProbe{ true, false, true, { 1, 2, 3, 4 } },
				RendererProbe{ true, true, false, { 1, 2, 3, 4 } },
				RendererProbe{ true, true, true, { 0, 2, 3, 4 } },
				RendererProbe{ true, true, true, { 1, 0, 3, 4 } },
				RendererProbe{ true, true, true, { 1, 2, 0, 4 } },
				RendererProbe{ true, true, true, { 1, 2, 3, 0 } }
			};
			for (const auto& probe : probes)
			{
				const auto observation = ObserveRenderer(probe);
				require(
					FailedAttachmentResult(observation) == AttachmentResult::kNotReady &&
						DearModdingUI::SwapChainAttachmentResult(
							FailedAttachmentResult(observation)) == DMUI_RESULT_HOST_NOT_READY,
					"missing renderer prerequisite became a permanent rejection");
			}
			require(
				DearModdingUI::SwapChainAttachmentResult(
					FailedAttachmentResult(RendererObservation::kBindingChanged)) ==
					DMUI_RESULT_RENDERER_BUSY,
				"binding publication race was not retryable");
			for (const auto observation : {
					 RendererObservation::kInvalidBinding,
					 RendererObservation::kHookInstallationFailed,
					 RendererObservation::kReady })
			{
				require(
					DearModdingUI::SwapChainAttachmentResult(
						FailedAttachmentResult(observation)) == DMUI_RESULT_SWAPCHAIN_REJECTED,
					"real or unclassified attachment failure was made retryable");
			}
			require(
				DearModdingUI::SwapChainAttachmentResult(AttachmentResult::kAttached) ==
					DMUI_RESULT_OK,
				"successful attachment no longer reports success");
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
			require(!ObservesDisplayedFrame(kPresentTestFlag, false), "a failed test Present displays no frame");
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

		runner.test("input hook health requires every receiver", [] {
			const std::array ready{
				InputReceiverHookOutcome{
					InputReceiver::kMenuControls,
					InputHookFailure::kNone },
				InputReceiverHookOutcome{
					InputReceiver::kPlayerControls,
					InputHookFailure::kNone },
				InputReceiverHookOutcome{
					InputReceiver::kPlayerCamera,
					InputHookFailure::kNone }
			};
			const auto observation = ClassifyInputHookHealth(ready);
			require(
				observation.state == DearModdingUI::HealthState::kReady &&
					observation.reason.find("PlayerCamera") !=
						std::string::npos,
				"input health reported ready without naming all required receivers");
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

		runner.test("input hook health identifies each required receiver", [] {
			constexpr std::array receivers{
				InputReceiver::kMenuControls,
				InputReceiver::kPlayerControls,
				InputReceiver::kPlayerCamera
			};
			for (size_t failed = 0; failed < receivers.size(); ++failed)
			{
				std::array outcomes{
					InputReceiverHookOutcome{
						InputReceiver::kMenuControls,
						InputHookFailure::kNone },
					InputReceiverHookOutcome{
						InputReceiver::kPlayerControls,
						InputHookFailure::kNone },
					InputReceiverHookOutcome{
						InputReceiver::kPlayerCamera,
						InputHookFailure::kNone }
				};
				outcomes[failed].failure =
					InputHookFailure::kSingletonUnavailable;
				const auto observation =
					ClassifyInputHookHealth(outcomes);
				require(
					observation.state ==
							DearModdingUI::HealthState::kFailed &&
						observation.reason.find(
							InputReceiverName(receivers[failed])) !=
							std::string::npos,
					"input health attributed a failure to the wrong receiver");
			}
		});

		runner.test("backbuffer state recreates on identity size and view changes", [] {
			constexpr BackBufferIdentity empty{};
			constexpr BackBufferIdentity first{ 1, 1920, 1080 };
			constexpr BackBufferIdentity replacement{ 2, 1920, 1080 };
			constexpr BackBufferIdentity resized{ 1, 2560, 1440 };
			constexpr BackBufferIdentity invalid{ 1, 0, 1080 };

			require(
				DecideBackBuffer(empty, first, false) == BackBufferDecision::kRecreate,
				"the first valid backbuffer must create an RTV");
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
			constexpr MousePosition equal{ 123.75f, 456.25f };
			constexpr auto equalMapped =
				MapClientToBackBuffer(equal, 1920, 1080, 1920, 1080);
			require(equalMapped.x == equal.x && equalMapped.y == equal.y,
				"equal dimensions changed mouse coordinates");

			constexpr auto uniform =
				MapClientToBackBuffer({ 480.0f, 270.0f }, 960, 540, 1920, 1080);
			require(uniform.x == 960.0f && uniform.y == 540.0f,
				"uniform scaling did not match the backbuffer");

			constexpr auto nonUniform =
				MapClientToBackBuffer({ 400.0f, 300.0f }, 800, 600, 2560, 1080);
			require(nonUniform.x == 1280.0f && nonUniform.y == 540.0f,
				"independent axis scaling changed");

			constexpr MousePosition position{ 400.0f, 300.0f };
			constexpr auto zeroClientWidth =
				MapClientToBackBuffer(position, 0, 600, 2560, 1080);
			constexpr auto zeroClientHeight =
				MapClientToBackBuffer(position, 800, 0, 2560, 1080);
			constexpr auto zeroBackBufferWidth =
				MapClientToBackBuffer(position, 800, 600, 0, 1080);
			constexpr auto zeroBackBufferHeight =
				MapClientToBackBuffer(position, 800, 600, 2560, 0);
			require(
				zeroClientWidth.x == position.x && zeroClientWidth.y == position.y &&
					zeroClientHeight.x == position.x && zeroClientHeight.y == position.y &&
					zeroBackBufferWidth.x == position.x && zeroBackBufferWidth.y == position.y &&
					zeroBackBufferHeight.x == position.x && zeroBackBufferHeight.y == position.y,
				"degenerate dimensions changed mouse coordinates");

			constexpr auto unavailable =
				-(std::numeric_limits<float>::max)();
			constexpr auto sentinel =
				MapClientToBackBuffer({ unavailable, unavailable }, 800, 600, 2560, 1080);
			require(sentinel.x == unavailable && sentinel.y == unavailable,
				"the unavailable mouse sentinel was scaled");

			constexpr MousePosition leftOutside{ -1.0f, 300.0f };
			constexpr MousePosition rightOutside{ 800.0f, 300.0f };
			constexpr MousePosition belowOutside{ 400.0f, 601.0f };
			constexpr auto leftMapped =
				MapClientToBackBuffer(leftOutside, 800, 600, 2560, 1080);
			constexpr auto rightMapped =
				MapClientToBackBuffer(rightOutside, 800, 600, 2560, 1080);
			constexpr auto belowMapped =
				MapClientToBackBuffer(belowOutside, 800, 600, 2560, 1080);
			require(
				leftMapped.x == leftOutside.x && leftMapped.y == leftOutside.y &&
					rightMapped.x == rightOutside.x && rightMapped.y == rightOutside.y &&
					belowMapped.x == belowOutside.x && belowMapped.y == belowOutside.y,
				"out-of-window mouse coordinates were scaled into the viewport");
		});

		runner.test("window messages are classified and swallowed by capture state", [] {
			require(ClassifyMessage(0x0100) == MessageClass::kKeyboard, "WM_KEYDOWN is keyboard");
			require(ClassifyMessage(0x0102) == MessageClass::kKeyboard, "WM_CHAR is keyboard");
			require(ClassifyMessage(0x0109) == MessageClass::kKeyboard, "the last keyboard message is keyboard");
			require(ClassifyMessage(0x0200) == MessageClass::kMouse, "WM_MOUSEMOVE is mouse");
			require(ClassifyMessage(0x020E) == MessageClass::kMouse, "WM_MOUSEHWHEEL is mouse");
			require(ClassifyMessage(0x00FF) == MessageClass::kOther, "WM_INPUT is neither");
			require(ClassifyMessage(0x0020) == MessageClass::kOther, "WM_SETCURSOR is neither");
			require(ClassifyMessage(0x010A) == MessageClass::kOther, "the message after the keyboard range is neither");
			require(ClassifyMessage(0x020F) == MessageClass::kOther, "the message after the mouse range is neither");

			require(SwallowsMessage(MessageClass::kMouse, true, false), "captured mouse input stops at the menu");
			require(!SwallowsMessage(MessageClass::kMouse, false, true), "uncaptured mouse input reaches the game");
			require(SwallowsMessage(MessageClass::kKeyboard, false, true), "captured keys stop at the menu");
			require(!SwallowsMessage(MessageClass::kKeyboard, true, false), "uncaptured keys reach the game");
			require(!SwallowsMessage(MessageClass::kOther, true, true), "other messages always reach the game");
		});

		runner.test("toggle callback fires once per physical press", [] {
			require(IsKeyRepeat(kKeyRepeatBit), "bit 30 marks an auto repeat");
			require(!IsKeyRepeat(0), "a first press carries no repeat bit");
			require(!IsKeyRepeat(0x0001), "the repeat count does not mark a repeat");
			require(IsKeyRepeat(kKeyRepeatBit | 0xC0000001ull), "release flags do not hide the repeat bit");

			require(DispatchesToggleCallback(0x0100, 0x0001), "a fresh WM_KEYDOWN dispatches");
			require(!DispatchesToggleCallback(0x0100, kKeyRepeatBit | 0x0001), "a held key does not redispatch");
			require(!DispatchesToggleCallback(0x0101, 0x0001), "WM_KEYUP does not dispatch");
			require(DispatchesToggleCallback(0x0104, 0x0001),
				"bare F10 arrives as WM_SYSKEYDOWN and must dispatch");
			require(!DispatchesToggleCallback(0x0104, kKeyRepeatBit | 0x0001),
				"a held system key does not redispatch");
			require(!DispatchesToggleCallback(0x0105, 0x0001), "WM_SYSKEYUP does not dispatch");
			require(ClassifyMessage(0x0104) == MessageClass::kKeyboard,
				"WM_SYSKEYDOWN is keyboard traffic and follows the capture state");

			require(
				DecideToggleMessage(kKeyDownMessage, 1, false) ==
					ToggleMessageDecision::kDispatch,
				"a fresh keydown must invoke the toggle callback");
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
				"a consumed system press must consume its system key-up");
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
					1,
					false,
					true) ==
					EscapeMessageDecision::kReleaseAndForward,
				"a new closed-host Escape inherited stale ownership");
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
					kKeyUpMessage,
					0x10,
					1,
					true,
					true) == EscapeMessageDecision::kForward,
				"captured Escape also consumed a modifier release");
			require(
				DecideEscapeMessage(
					kKeyDownMessage,
					0x23,
					1,
					true,
					false) == EscapeMessageDecision::kForward,
				"the existing End toggle was reclassified as Escape");
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
