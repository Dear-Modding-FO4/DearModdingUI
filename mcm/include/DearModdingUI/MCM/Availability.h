#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <cstdint>

namespace DearModdingUI::MCM
{
	enum class AvailabilityState : uint8_t
	{
		kUnknown,
		kPresent,
		kAbsent
	};

	[[nodiscard]] constexpr bool IsControlOperable(
		AvailabilityState a_availability,
		SourceFamily a_family) noexcept
	{
		switch (a_family)
		{
		case SourceFamily::kGlobal:
		case SourceFamily::kProperty:
			return true;
		case SourceFamily::kModSetting:
			return a_availability == AvailabilityState::kPresent;
		case SourceFamily::kUnknown:
			return false;
		}
		return false;
	}
}
