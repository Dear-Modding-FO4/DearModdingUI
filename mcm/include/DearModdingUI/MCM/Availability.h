#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <functional>

namespace DearModdingUI::MCM
{
	struct McmState
	{
		bool installed{};
		bool runtimeReady{};

		bool operator==(const McmState&) const = default;
	};

	[[nodiscard]] constexpr InertReason ResolveControlInertReason(
		McmState a_state,
		SourceFamily a_family,
		ValueRoute a_route = ValueRoute::kSource) noexcept
	{
		if (a_route == ValueRoute::kLocalUiState)
			return InertReason::kNone;
		switch (a_family)
		{
		case SourceFamily::kGlobal:
			return InertReason::kNone;
		case SourceFamily::kProperty:
			return a_state.runtimeReady ?
				InertReason::kNone :
				InertReason::kRuntimeNotReady;
		case SourceFamily::kModSetting:
			if (!a_state.installed)
				return InertReason::kMcmNotInstalled;
			return a_state.runtimeReady ?
				InertReason::kNone :
				InertReason::kRuntimeNotReady;
		case SourceFamily::kUnknown:
			return InertReason::kUnsupported;
		}
		return InertReason::kUnsupported;
	}

	[[nodiscard]] constexpr bool IsControlOperable(
		McmState a_state,
		SourceFamily a_family,
		ValueRoute a_route = ValueRoute::kSource) noexcept
	{
		return ResolveControlInertReason(a_state, a_family, a_route) ==
			InertReason::kNone;
	}

	using McmStateResolver = std::function<McmState()>;
}
