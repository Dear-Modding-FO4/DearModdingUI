#pragma once

#include <Support/SubsystemHealth.h>

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace Addictol::GameInput
{
	inline constexpr uint32_t kPerformInputProcessingSlot = 0;
	inline constexpr uintptr_t kPlayerCameraReceiverOffset = 0x38;

	enum class InputQueueDecision : uint32_t
	{
		kForward,
		kDiscard
	};

	enum class InputSuppressionPolicy : uint32_t
	{
		kAllDevices
	};

	inline constexpr auto kMenuInputSuppression = InputSuppressionPolicy::kAllDevices;

	enum class InputReceiver : uint32_t
	{
		kMenuControls,
		kPlayerControls,
		kPlayerCamera
	};

	enum class InputHookFailure : uint32_t
	{
		kNone,
		kSingletonUnavailable,
		kReceiverOffsetMismatch,
		kVtableUnavailable,
		kTargetUnavailable,
		kAlreadyHooked,
		kPatchFailed,
		kOriginalTargetLost
	};

	struct InputReceiverHookOutcome
	{
		InputReceiver receiver{ InputReceiver::kMenuControls };
		InputHookFailure failure{ InputHookFailure::kNone };
	};

	[[nodiscard]] constexpr std::string_view InputReceiverName(
		InputReceiver a_receiver) noexcept
	{
		switch (a_receiver)
		{
		case InputReceiver::kMenuControls:
			return "MenuControls";
		case InputReceiver::kPlayerControls:
			return "PlayerControls";
		case InputReceiver::kPlayerCamera:
			return "PlayerCamera";
		default:
			return "Unknown receiver";
		}
	}

	[[nodiscard]] inline DearModdingUI::HealthObservation
		ClassifyInputHookHealth(
			const std::array<InputReceiverHookOutcome, 3>& a_outcomes)
	{
		for (const auto& outcome : a_outcomes)
		{
			if (outcome.failure == InputHookFailure::kNone)
				continue;

			std::string detail;
			switch (outcome.failure)
			{
			case InputHookFailure::kSingletonUnavailable:
				detail =
					"singleton is unavailable; restart after game data is ready";
				break;
			case InputHookFailure::kReceiverOffsetMismatch:
				detail =
					"receiver layout did not match this runtime; verify the game and plugin versions";
				break;
			case InputHookFailure::kVtableUnavailable:
				detail =
					"receiver vtable is unavailable; restart the game";
				break;
			case InputHookFailure::kTargetUnavailable:
				detail =
					"PerformInputProcessing target is unavailable; verify the game runtime";
				break;
			case InputHookFailure::kAlreadyHooked:
				detail =
					"receiver was already redirected; check for an incompatible input hook";
				break;
			case InputHookFailure::kPatchFailed:
				detail =
					"receiver patch failed; check for an incompatible input hook";
				break;
			case InputHookFailure::kOriginalTargetLost:
				detail =
					"hook lost its original target; restart the game";
				break;
			default:
				detail = "interception failed";
				break;
			}
			return {
				DearModdingUI::HealthState::kFailed,
				std::format(
					"{} {}. Required game-input interception is unavailable.",
					InputReceiverName(outcome.receiver),
					detail)
			};
		}
		return {
			DearModdingUI::HealthState::kReady,
			"MenuControls, PlayerControls, and PlayerCamera interception installed."
		};
	}

	[[nodiscard]] constexpr InputQueueDecision DecideInputQueue(
		bool a_menuVisible,
		InputSuppressionPolicy a_policy = kMenuInputSuppression) noexcept
	{
		return a_menuVisible && a_policy == InputSuppressionPolicy::kAllDevices ?
			InputQueueDecision::kDiscard :
			InputQueueDecision::kForward;
	}

	[[nodiscard]] constexpr bool MatchesReceiverOffset(
		uintptr_t a_object,
		uintptr_t a_receiver,
		uintptr_t a_expectedOffset) noexcept
	{
		return a_object &&
			a_receiver >= a_object &&
			a_receiver - a_object == a_expectedOffset;
	}

	[[nodiscard]] bool InstallHooks() noexcept;
	void InitializeHealth() noexcept;
	void SetBlocked(bool a_blocked) noexcept;
}
