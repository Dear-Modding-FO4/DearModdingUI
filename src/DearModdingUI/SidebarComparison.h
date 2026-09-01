#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace DearModdingUI
{
	enum class PreviewSidebarLayout : uint32_t
	{
		kTree,
		kTwoPane
	};

	struct SidebarPaneHeights
	{
		float mods{ 0.0f };
		float pages{ 0.0f };

		constexpr bool operator==(const SidebarPaneHeights&) const noexcept = default;
	};

	[[nodiscard]] constexpr SidebarPaneHeights ResolveSidebarPaneHeights(
		float a_availableHeight,
		float a_modRowStride,
		size_t a_modCount,
		float a_minimumPagesHeight) noexcept
	{
		const auto available = (std::max)(a_availableHeight, 0.0f);
		const auto rowStride = (std::max)(a_modRowStride, 0.0f);
		const auto minimumPages = (std::min)(
			(std::max)(a_minimumPagesHeight, 0.0f),
			available);
		const auto desiredMods =
			rowStride * static_cast<float>(a_modCount);
		const auto mods = (std::min)(
			desiredMods,
			available - minimumPages);
		return { mods, available - mods };
	}

	void ConfigurePreviewSidebarComparison(
		PreviewSidebarLayout a_layout,
		bool a_overrideExpandedClients,
		std::span<const std::string> a_expandedClients);
}
