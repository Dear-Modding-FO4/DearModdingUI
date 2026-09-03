#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <cstdint>
#include <functional>
#include <string_view>

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
		SourceFamily a_family,
		ValueRoute a_route = ValueRoute::kSource) noexcept
	{
		if (a_route == ValueRoute::kLocalUiState)
			return true;
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

	[[nodiscard]] constexpr std::string_view ControlUnavailableReason(
		AvailabilityState a_availability,
		SourceFamily a_family,
		ValueRoute a_route = ValueRoute::kSource) noexcept
	{
		if (IsControlOperable(a_availability, a_family, a_route))
			return {};
		if (a_family == SourceFamily::kModSetting)
		{
			return a_availability == AvailabilityState::kAbsent ?
				"Mod Configuration Menu is not installed." :
				"Mod Configuration Menu availability has not been determined yet.";
		}
		return "This setting's value source is unavailable.";
	}

	using AvailabilityResolver = std::function<AvailabilityState()>;

	void ComposeMcmAvailability(
		MappedPage& a_page,
		AvailabilityResolver a_resolve);
}
