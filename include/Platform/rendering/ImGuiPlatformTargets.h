#pragma once

#include <cstdint>

namespace Addictol::ImguiPlatform
{
	inline constexpr uint32_t kPresentSlot = 8;
	inline constexpr uint32_t kResizeBuffersSlot = 13;
	inline constexpr uint32_t kPresentTestFlag = 0x00000001;
	inline constexpr uint32_t kWindowNcDestroyMessage = 0x0082;

	struct AttachmentIdentity
	{
		uintptr_t swapChain{ 0 };
		uintptr_t device{ 0 };
		uintptr_t context{ 0 };
		uintptr_t window{ 0 };

		[[nodiscard]] constexpr bool Valid() const noexcept
		{
			return swapChain && device && context && window;
		}
	};

	enum class AttachmentSource : uint32_t
	{
		kRenderer,
		kExplicit
	};

	enum class AttachmentDecision : uint32_t
	{
		kReject,
		kKeepCurrent,
		kAttach,
		kReplace
	};

	enum class AttachmentLifecycle : uint32_t
	{
		kVacant,
		kActive,
		kRetired
	};

	[[nodiscard]] constexpr AttachmentDecision DecideAttachment(
		const AttachmentIdentity& a_current,
		const AttachmentIdentity& a_candidate,
		AttachmentSource a_currentSource,
		AttachmentSource a_candidateSource,
		AttachmentLifecycle a_lifecycle) noexcept
	{
		if (!a_candidate.Valid())
			return AttachmentDecision::kReject;
		if (a_lifecycle != AttachmentLifecycle::kActive || !a_current.Valid())
			return AttachmentDecision::kAttach;
		if (a_current.swapChain == a_candidate.swapChain &&
			a_current.device == a_candidate.device &&
			a_current.context == a_candidate.context &&
			a_current.window == a_candidate.window)
			return AttachmentDecision::kKeepCurrent;
		if (a_currentSource == AttachmentSource::kExplicit &&
			a_candidateSource == AttachmentSource::kRenderer &&
			a_current.device == a_candidate.device &&
			a_current.context == a_candidate.context &&
			a_current.window == a_candidate.window)
			return AttachmentDecision::kKeepCurrent;
		return AttachmentDecision::kReplace;
	}

	enum class RendererObservation : uint32_t
	{
		kReady,
		kRendererDataMissing,
		kRendererNotInitialized,
		kRendererWindowMissing,
		kSwapChainMissing,
		kDeviceMissing,
		kContextMissing,
		kWindowMissing,
		kBindingChanged,
		kInvalidBinding,
		kHookInstallationFailed
	};

	enum class AttachmentResult : uint32_t
	{
		kAttached,
		kNotReady,
		kBusy,
		kRejected
	};

	[[nodiscard]] constexpr AttachmentResult FailedAttachmentResult(
		RendererObservation a_observation) noexcept
	{
		switch (a_observation)
		{
		case RendererObservation::kRendererDataMissing:
		case RendererObservation::kRendererNotInitialized:
		case RendererObservation::kRendererWindowMissing:
		case RendererObservation::kSwapChainMissing:
		case RendererObservation::kDeviceMissing:
		case RendererObservation::kContextMissing:
		case RendererObservation::kWindowMissing:
			return AttachmentResult::kNotReady;
		case RendererObservation::kBindingChanged:
			return AttachmentResult::kBusy;
		default:
			return AttachmentResult::kRejected;
		}
	}

	struct RendererProbe
	{
		bool hasRendererData{ false };
		bool initialized{ false };
		bool hasRendererWindow{ false };
		AttachmentIdentity binding{};
	};

	[[nodiscard]] constexpr RendererObservation ObserveRenderer(
		const RendererProbe& a_probe) noexcept
	{
		if (!a_probe.hasRendererData)
			return RendererObservation::kRendererDataMissing;
		if (!a_probe.initialized)
			return RendererObservation::kRendererNotInitialized;
		if (!a_probe.hasRendererWindow)
			return RendererObservation::kRendererWindowMissing;
		if (!a_probe.binding.swapChain)
			return RendererObservation::kSwapChainMissing;
		if (!a_probe.binding.device)
			return RendererObservation::kDeviceMissing;
		if (!a_probe.binding.context)
			return RendererObservation::kContextMissing;
		if (!a_probe.binding.window)
			return RendererObservation::kWindowMissing;
		return RendererObservation::kReady;
	}

	inline constexpr uint32_t kDxgiErrorDeviceRemoved = 0x887A0005u;
	inline constexpr uint32_t kDxgiErrorDeviceHung = 0x887A0006u;
	inline constexpr uint32_t kDxgiErrorDeviceReset = 0x887A0007u;
	inline constexpr uint32_t kDxgiErrorDriverInternal = 0x887A0020u;

	[[nodiscard]] constexpr bool IsDefinitiveSwapChainLoss(uint32_t a_result) noexcept
	{
		return a_result == kDxgiErrorDeviceRemoved ||
			a_result == kDxgiErrorDeviceHung ||
			a_result == kDxgiErrorDeviceReset ||
			a_result == kDxgiErrorDriverInternal;
	}

	[[nodiscard]] constexpr bool ReusesHookAssociation(
		AttachmentLifecycle a_lifecycle,
		uintptr_t a_liveVtable,
		uintptr_t a_associatedVtable) noexcept
	{
		return a_lifecycle != AttachmentLifecycle::kRetired ||
			(a_liveVtable && a_liveVtable == a_associatedVtable);
	}

	enum class HookDispatchMatch : uint32_t
	{
		kNone,
		kSwapChain,
		kVtable
	};

	[[nodiscard]] constexpr HookDispatchMatch MatchHookDispatch(
		uintptr_t a_swapChain,
		uintptr_t a_liveVtable,
		uintptr_t a_associatedSwapChain,
		uintptr_t a_patchedVtable) noexcept
	{
		if (a_associatedSwapChain && a_swapChain == a_associatedSwapChain)
			return HookDispatchMatch::kSwapChain;
		if (a_patchedVtable && a_liveVtable == a_patchedVtable)
			return HookDispatchMatch::kVtable;
		return HookDispatchMatch::kNone;
	}

	[[nodiscard]] constexpr bool ObservesDisplayedFrame(
		uint32_t a_presentFlags,
		bool a_presentSucceeded) noexcept
	{
		return a_presentSucceeded && (a_presentFlags & kPresentTestFlag) == 0;
	}

	struct PresentAttachmentToken
	{
		uintptr_t swapChain{};
		uint64_t generation{};

		[[nodiscard]] constexpr bool Valid() const noexcept
		{
			return swapChain && generation;
		}
	};

	[[nodiscard]] constexpr bool MatchesActivePresentAttachment(
		const PresentAttachmentToken& a_presented,
		uintptr_t a_activeSwapChain,
		uint64_t a_activeGeneration,
		AttachmentLifecycle a_lifecycle) noexcept
	{
		return a_presented.Valid() &&
			a_lifecycle == AttachmentLifecycle::kActive &&
			a_presented.swapChain == a_activeSwapChain &&
			a_presented.generation == a_activeGeneration;
	}

	[[nodiscard]] constexpr bool ShouldInitializeHost(
		bool a_windowReady) noexcept
	{
		return a_windowReady;
	}

	[[nodiscard]] constexpr bool ShouldRenderHostFrame(
		bool a_modalVisible,
		bool a_overlayDemanded) noexcept
	{
		return a_modalVisible || a_overlayDemanded;
	}

	[[nodiscard]] constexpr bool ShouldSuppressGameInput(bool a_modalVisible) noexcept
	{
		return a_modalVisible;
	}

	[[nodiscard]] constexpr bool RequiresBackendReset(
		const AttachmentIdentity& a_current,
		const AttachmentIdentity& a_candidate,
		bool a_backendInitialized) noexcept
	{
		return a_backendInitialized &&
			(a_current.device != a_candidate.device ||
				a_current.context != a_candidate.context ||
				a_current.window != a_candidate.window);
	}

	struct BackBufferIdentity
	{
		uintptr_t resource{ 0 };
		uint32_t width{ 0 };
		uint32_t height{ 0 };

		[[nodiscard]] constexpr bool Valid() const noexcept
		{
			return resource && width && height;
		}
	};

	struct MousePosition
	{
		float x{ 0.0f };
		float y{ 0.0f };
	};

	[[nodiscard]] constexpr MousePosition MapNativeCursorToBackBuffer(
		MousePosition a_position,
		uint32_t a_clientWidth,
		uint32_t a_clientHeight,
		uint32_t a_backBufferWidth,
		uint32_t a_backBufferHeight) noexcept
	{
		return {
			a_position.x * static_cast<float>(a_backBufferWidth) /
				static_cast<float>(a_clientWidth),
			a_position.y * static_cast<float>(a_backBufferHeight) /
				static_cast<float>(a_clientHeight)
		};
	}

	[[nodiscard]] constexpr MousePosition MapClientToBackBuffer(
		MousePosition a_position,
		uint32_t a_clientWidth,
		uint32_t a_clientHeight,
		uint32_t a_backBufferWidth,
		uint32_t a_backBufferHeight) noexcept
	{
		if (!a_clientWidth ||
			!a_clientHeight ||
			!a_backBufferWidth ||
			!a_backBufferHeight ||
			(a_clientWidth == a_backBufferWidth &&
				a_clientHeight == a_backBufferHeight) ||
			!(a_position.x >= 0.0f &&
				a_position.y >= 0.0f &&
				a_position.x < static_cast<float>(a_clientWidth) &&
				a_position.y < static_cast<float>(a_clientHeight)))
			return a_position;

		return MapNativeCursorToBackBuffer(
			a_position, a_clientWidth, a_clientHeight, a_backBufferWidth, a_backBufferHeight);
	}

	enum class BackBufferDecision : uint32_t
	{
		kSkip,
		kKeep,
		kRecreate
	};

	[[nodiscard]] constexpr BackBufferDecision DecideBackBuffer(
		const BackBufferIdentity& a_current,
		const BackBufferIdentity& a_candidate,
		bool a_hasRenderTarget) noexcept
	{
		if (!a_candidate.Valid())
			return BackBufferDecision::kSkip;
		if (a_hasRenderTarget &&
			a_current.resource == a_candidate.resource &&
			a_current.width == a_candidate.width &&
			a_current.height == a_candidate.height)
			return BackBufferDecision::kKeep;
		return BackBufferDecision::kRecreate;
	}

	enum class InstallState : uint32_t
	{
		kNotAttempted,
		kAttempted,
		kInstalled,
		kRejected
	};

	// Installation is attempted once so the permanent reconciliation task is not duplicated.
	[[nodiscard]] constexpr bool AllowsInstallAttempt(InstallState a_state) noexcept
	{
		return a_state == InstallState::kNotAttempted;
	}

	[[nodiscard]] constexpr bool IsInstalled(InstallState a_state) noexcept
	{
		return a_state == InstallState::kInstalled;
	}

	enum class MessageClass : uint32_t
	{
		kOther,
		kMouse,
		kKeyboard
	};

	// Win32 message numbers, spelled out so the pure input logic stays testable without Windows headers.
	inline constexpr uint32_t kKeyboardMessageFirst = 0x0100;
	inline constexpr uint32_t kKeyboardMessageLast = 0x0109;
	inline constexpr uint32_t kMouseMessageFirst = 0x0200;
	inline constexpr uint32_t kMouseMessageLast = 0x020E;
	inline constexpr uint32_t kKeyDownMessage = 0x0100;
	inline constexpr uint32_t kKeyUpMessage = 0x0101;
	inline constexpr uint32_t kSysKeyDownMessage = 0x0104;
	inline constexpr uint32_t kSysKeyUpMessage = 0x0105;
	inline constexpr uint64_t kKeyRepeatBit = uint64_t{ 1 } << 30;

	[[nodiscard]] constexpr MessageClass ClassifyMessage(uint32_t a_message) noexcept
	{
		if (a_message >= kMouseMessageFirst && a_message <= kMouseMessageLast)
			return MessageClass::kMouse;
		if (a_message >= kKeyboardMessageFirst && a_message <= kKeyboardMessageLast)
			return MessageClass::kKeyboard;
		return MessageClass::kOther;
	}

	// Everything the menu does not capture still reaches the game, including focus and system messages.
	[[nodiscard]] constexpr bool SwallowsMessage(
		MessageClass a_class,
		bool a_wantCaptureMouse,
		bool a_wantCaptureKeyboard) noexcept
	{
		switch (a_class)
		{
		case MessageClass::kMouse:
			return a_wantCaptureMouse;
		case MessageClass::kKeyboard:
			return a_wantCaptureKeyboard;
		default:
			return false;
		}
	}

	[[nodiscard]] constexpr bool SwallowsGameWindowMessage(
		uint32_t a_message,
		bool a_wantCaptureMouse,
		bool a_wantCaptureKeyboard) noexcept
	{
		// The native cursor consumes the original WM_MOUSEMOVE client coordinates.
		return a_message != kMouseMessageFirst &&
			SwallowsMessage(ClassifyMessage(a_message), a_wantCaptureMouse, a_wantCaptureKeyboard);
	}

	[[nodiscard]] constexpr bool IsKeyRepeat(uint64_t a_lparam) noexcept
	{
		return (a_lparam & kKeyRepeatBit) != 0;
	}

	// Fresh WM_KEYDOWN and WM_SYSKEYDOWN messages both dispatch, while auto repeat does not.
	[[nodiscard]] constexpr bool DispatchesToggleCallback(
		uint32_t a_message,
		uint64_t a_lparam) noexcept
	{
		return (a_message == kKeyDownMessage || a_message == kSysKeyDownMessage) && !IsKeyRepeat(a_lparam);
	}

	enum class ToggleMessageDecision : uint32_t
	{
		kForward,
		kDispatch,
		kConsume,
		kConsumeAndRelease
	};

	[[nodiscard]] constexpr ToggleMessageDecision DecideToggleMessage(
		uint32_t a_message,
		uint64_t a_lparam,
		bool a_pressConsumed) noexcept
	{
		if (a_message == kKeyDownMessage || a_message == kSysKeyDownMessage)
		{
			if (!IsKeyRepeat(a_lparam))
				return ToggleMessageDecision::kDispatch;
			return a_pressConsumed ?
				ToggleMessageDecision::kConsume :
				ToggleMessageDecision::kForward;
		}
		if ((a_message == kKeyUpMessage || a_message == kSysKeyUpMessage) &&
			a_pressConsumed)
			return ToggleMessageDecision::kConsumeAndRelease;
		return ToggleMessageDecision::kForward;
	}

	inline constexpr uint32_t kEscapeVirtualKey = 0x1B;

	enum class EscapeMessageDecision : uint32_t
	{
		kForward,
		kCapture,
		kConsume,
		kConsumeAndRelease,
		kReleaseAndForward
	};

	[[nodiscard]] constexpr EscapeMessageDecision DecideEscapeMessage(
		uint32_t a_message,
		uint32_t a_virtualKey,
		uint64_t a_lparam,
		bool a_menuVisible,
		bool a_pressConsumed) noexcept
	{
		if (a_virtualKey != kEscapeVirtualKey)
			return EscapeMessageDecision::kForward;
		if (a_message == kKeyDownMessage ||
			a_message == kSysKeyDownMessage)
		{
			if (IsKeyRepeat(a_lparam))
				return a_pressConsumed ?
					EscapeMessageDecision::kConsume :
					EscapeMessageDecision::kForward;
			if (a_menuVisible)
				return EscapeMessageDecision::kCapture;
			return a_pressConsumed ?
				EscapeMessageDecision::kReleaseAndForward :
				EscapeMessageDecision::kForward;
		}
		if ((a_message == kKeyUpMessage ||
				a_message == kSysKeyUpMessage) &&
			a_pressConsumed)
			return EscapeMessageDecision::kConsumeAndRelease;
		return EscapeMessageDecision::kForward;
	}

	[[nodiscard]] constexpr bool HandlesWindowMessage(
		bool a_activeWindow,
		bool a_drawingRequested,
		bool a_backendReady,
		bool a_hasContext) noexcept
	{
		return a_activeWindow &&
			a_drawingRequested &&
			a_backendReady &&
			a_hasContext;
	}

	[[nodiscard]] constexpr bool RetiresWindowHook(uint32_t a_message) noexcept
	{
		return a_message == kWindowNcDestroyMessage;
	}
}
