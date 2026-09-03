#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <cstdint>
#include <functional>
#include <string_view>

namespace DearModdingUI::MCM
{
	struct McmState
	{
		bool installed{};
		bool runtimeReady{};

		bool operator==(const McmState&) const = default;
	};

	[[nodiscard]] constexpr bool IsControlOperable(
		McmState a_state,
		SourceFamily a_family,
		ValueRoute a_route = ValueRoute::kSource) noexcept
	{
		if (a_route == ValueRoute::kLocalUiState)
			return true;
		switch (a_family)
		{
		case SourceFamily::kGlobal:
			return true;
		case SourceFamily::kProperty:
			return a_state.runtimeReady;
		case SourceFamily::kModSetting:
			return a_state.installed && a_state.runtimeReady;
		case SourceFamily::kUnknown:
			return false;
		}
		return false;
	}

	[[nodiscard]] constexpr std::string_view ControlUnavailableReason(
		McmState a_state,
		SourceFamily a_family,
		ValueRoute a_route = ValueRoute::kSource) noexcept
	{
		if (IsControlOperable(a_state, a_family, a_route))
			return {};
		if (a_family == SourceFamily::kModSetting && !a_state.installed)
			return "Mod Configuration Menu is not installed.";
		if ((a_family == SourceFamily::kModSetting ||
				a_family == SourceFamily::kProperty) &&
			!a_state.runtimeReady)
			return "Load a save to change these settings.";
		return "This setting's value source is unavailable.";
	}

	using McmStateResolver = std::function<McmState()>;
}
