#pragma once

#include <Support/Message.h>
#include <Support/SubsystemHealth.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace DearModdingUI::GameInput
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

	// Receiver names stay literal class names inside one translated sentence.
	[[nodiscard]] inline Support::Message AllReceiversMessage(
		const Support::Phrase& a_phrase)
	{
		return Support::Message{
			a_phrase,
			InputReceiverName(InputReceiver::kMenuControls),
			InputReceiverName(InputReceiver::kPlayerControls),
			InputReceiverName(InputReceiver::kPlayerCamera)
		};
	}

	[[nodiscard]] constexpr Support::Phrase InputFailureClause(
		InputHookFailure a_failure) noexcept
	{
		switch (a_failure)
		{
		case InputHookFailure::kSingletonUnavailable:
			return { "$DMUI_Input_SingletonUnavailable", "{} singleton is unavailable; restart after game data is ready." };
		case InputHookFailure::kReceiverOffsetMismatch:
			return { "$DMUI_Input_ReceiverOffsetMismatch", "{} receiver layout did not match this runtime; verify the game and plugin versions." };
		case InputHookFailure::kVtableUnavailable:
			return { "$DMUI_Input_VtableUnavailable", "{} receiver vtable is unavailable; restart the game." };
		case InputHookFailure::kTargetUnavailable:
			return { "$DMUI_Input_TargetUnavailable", "{} PerformInputProcessing target is unavailable; verify the game runtime." };
		case InputHookFailure::kAlreadyHooked:
			return { "$DMUI_Input_AlreadyHooked", "{} receiver was already redirected; check for an incompatible input hook." };
		case InputHookFailure::kPatchFailed:
			return { "$DMUI_Input_PatchFailed", "{} receiver patch failed; check for an incompatible input hook." };
		case InputHookFailure::kOriginalTargetLost:
			return { "$DMUI_Input_OriginalTargetLost", "{} hook lost its original target; restart the game." };
		default:
			return { "$DMUI_Input_InterceptionFailed", "{} interception failed." };
		}
	}

	[[nodiscard]] inline DearModdingUI::HealthObservation
		ClassifyInputHookHealth(
			const std::array<InputReceiverHookOutcome, 3>& a_outcomes)
	{
		using Support::Message;

		for (const auto& outcome : a_outcomes)
		{
			if (outcome.failure == InputHookFailure::kNone)
				continue;

			return {
				DearModdingUI::HealthState::kFailed,
				Message::Join(
					{ Message{ InputFailureClause(outcome.failure), InputReceiverName(outcome.receiver) },
						Message{ Support::Phrase{
							"$DMUI_Input_InterceptionUnavailable",
							"Required game-input interception is unavailable." } } },
					" ")
			};
		}
		return {
			DearModdingUI::HealthState::kReady,
			AllReceiversMessage({
				"$DMUI_Input_Ready",
				"{}, {}, and {} interception installed."
			})
		};
	}

	[[nodiscard]] constexpr InputQueueDecision DecideInputQueue(
		bool a_menuVisible,
		InputSuppressionPolicy a_policy = kMenuInputSuppression,
		bool a_hostInputBlock = false) noexcept
	{
		return (a_menuVisible || a_hostInputBlock) && a_policy == InputSuppressionPolicy::kAllDevices ?
			InputQueueDecision::kDiscard :
			InputQueueDecision::kForward;
	}

	// Non-extended navigation keys arrive from the numpad.
	inline constexpr std::array<std::pair<uint32_t, uint32_t>, 11> kNumpadNavigation{ {
		{ 0x0C, 0x65 }, { 0x21, 0x69 }, { 0x22, 0x63 }, { 0x23, 0x61 },
		{ 0x24, 0x67 }, { 0x25, 0x64 }, { 0x26, 0x68 }, { 0x27, 0x66 },
		{ 0x28, 0x62 }, { 0x2D, 0x60 }, { 0x2E, 0x6E },
	} };

	// The engine keys keyboard state by virtual key after its own remaps.
	[[nodiscard]] constexpr uint32_t EngineKeyboardCode(uint32_t a_virtualKey, bool a_extended) noexcept
	{
		if (a_virtualKey == 0x11 || a_virtualKey == 0x12)
			return (a_virtualKey == 0x11 ? 0xA2u : 0xA4u) + (a_extended ? 1u : 0u);
		if (!a_extended)
			for (const auto [navigation, numpad] : kNumpadNavigation)
				if (navigation == a_virtualKey)
					return numpad;
		return a_virtualKey;
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
	void NoteKeyboardEdge(uint32_t a_engineCode, bool a_pressed, bool a_consumed) noexcept;
}
