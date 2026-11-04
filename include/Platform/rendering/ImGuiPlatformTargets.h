#pragma once

#include <cstdint>
#include <Support/KeyCatalog.h>

namespace DearModdingUI::PlatformImGui
{
	inline constexpr uint32_t kPresentSlot = 8;
	inline constexpr uint32_t kResizeBuffersSlot = 13;
	inline constexpr uint32_t kPresentTestFlag = 0x00000001;

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

		[[nodiscard]] constexpr bool operator==(const AttachmentIdentity&) const noexcept = default;
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
		if (a_current == a_candidate)
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

		[[nodiscard]] constexpr bool operator==(const RendererProbe&) const noexcept = default;
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

	// Retirement clears the committed probe, so only an unchanged live binding skips the engine lock.
	[[nodiscard]] constexpr bool RequiresRendererReconciliation(
		const RendererProbe& a_reconciled,
		const RendererProbe& a_published) noexcept
	{
		return ObserveRenderer(a_published) != RendererObservation::kReady ||
			a_published != a_reconciled;
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

	enum class Readiness : uint32_t
	{
		kReady,
		kRendererNotAttached,
		kBackendUninitialized,
		kBackendFailed
	};

	[[nodiscard]] constexpr Readiness ClassifyReadiness(
		bool a_attached,
		bool a_backendReady,
		bool a_backendFailed) noexcept
	{
		if (!a_attached)
			return Readiness::kRendererNotAttached;
		if (a_backendFailed)
			return Readiness::kBackendFailed;
		return a_backendReady ? Readiness::kReady : Readiness::kBackendUninitialized;
	}

	[[nodiscard]] constexpr bool ShouldRenderHostFrame(
		bool a_modalVisible,
		bool a_overlayDemanded) noexcept
	{
		return a_modalVisible || a_overlayDemanded;
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

	[[nodiscard]] constexpr uint32_t MouseKeyCode(uint32_t a_idCode) noexcept
	{
		if (a_idCode < DearModdingUI::KeyCatalog::kMouseButtonCount)
			return DearModdingUI::KeyCatalog::kMouseButtonOffset + a_idCode;
		if (a_idCode == 0x800)
			return DearModdingUI::KeyCatalog::kMouseWheelOffset;
		if (a_idCode == 0x900)
			return DearModdingUI::KeyCatalog::kMouseWheelOffset + 1;
		return 0;
	}

	[[nodiscard]] constexpr bool IsMouseWheelCode(uint32_t a_code) noexcept
	{
		return a_code == DearModdingUI::KeyCatalog::kMouseWheelOffset ||
			a_code == DearModdingUI::KeyCatalog::kMouseWheelOffset + 1;
	}

	inline constexpr uint32_t kEscapeVirtualKey = 0x1B;
	inline constexpr uint32_t kEscapeKeyCode = 0x01;

	enum class EscapeDecision : uint32_t
	{
		kForward,
		kCapture,
		kConsume,
		kConsumeAndRelease,
		kReleaseAndForward
	};

	// An Escape press the visible host captured is owned until its release.
	[[nodiscard]] constexpr EscapeDecision DecideEscapeEdge(
		bool a_pressed,
		bool a_repeat,
		bool a_menuVisible,
		bool a_pressConsumed) noexcept
	{
		if (!a_pressed)
			return a_pressConsumed ? EscapeDecision::kConsumeAndRelease : EscapeDecision::kForward;
		if (a_repeat)
			return a_pressConsumed ? EscapeDecision::kConsume : EscapeDecision::kForward;
		if (a_menuVisible)
			return EscapeDecision::kCapture;
		return a_pressConsumed ? EscapeDecision::kReleaseAndForward : EscapeDecision::kForward;
	}
}
