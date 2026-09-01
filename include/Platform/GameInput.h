#pragma once

#include <cstdint>

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
	void SetBlocked(bool a_blocked) noexcept;
}
