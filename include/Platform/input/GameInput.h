#pragma once

#include <Support/Message.h>
#include <Support/SubsystemHealth.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace DearModdingUI::GameInput
{
	inline constexpr uint32_t kPerformInputProcessingSlot = 0;
	// BSInputDevice::ClearInputState; the engine runs it on focus change.
	inline constexpr uint32_t kClearInputStateSlot = 5;
	inline constexpr uintptr_t kPlayerCameraReceiverOffset = 0x38;

	enum class InputReceiver : uint32_t
	{
		kMenuControls,
		kPlayerControls,
		kPlayerCamera,
		kKeyboardDevice
	};

	inline constexpr size_t kInputHookCount = 4;

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
		case InputReceiver::kKeyboardDevice:
			return "KeyboardDevice";
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
			InputReceiverName(InputReceiver::kPlayerCamera),
			InputReceiverName(InputReceiver::kKeyboardDevice)
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
			const std::array<InputReceiverHookOutcome, kInputHookCount>& a_outcomes)
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
				"{}, {}, {}, and {} interception installed."
			})
		};
	}

	// A held toggle chord stays DMUI's after the menu closes.
	[[nodiscard]] constexpr bool SuppressesGameInput(
		bool a_menuOwnsInput,
		bool a_toggleChordHeld) noexcept
	{
		return a_menuOwnsInput || a_toggleChordHeld;
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
	// False until all four hooks installed.
	[[nodiscard]] bool HooksInstalled() noexcept;
	void PublishThreadTapHealth(uint32_t a_thread, bool a_installed) noexcept;
	void InitializeHealth() noexcept;
	// Marks button, character and stick events kStop so no receiver sees them.
	void SuppressGameInput(bool a_suppressed) noexcept;
	// Engine-queue modifier state as kHotkeyModifier* bits.
	[[nodiscard]] uint32_t HeldModifiers() noexcept;
}
