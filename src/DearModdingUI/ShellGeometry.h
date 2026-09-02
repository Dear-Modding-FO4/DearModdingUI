#pragma once

#include <DearModdingUI/VisualDecisions.h>

namespace DearModdingUI
{
	struct HorizontalRuleSegment
	{
		float minX{ 0.0f };
		float maxX{ 0.0f };

		constexpr bool operator==(const HorizontalRuleSegment&) const noexcept = default;
	};

	struct RuledHeadingRuleExtents
	{
		HorizontalRuleSegment left;
		HorizontalRuleSegment right;

		constexpr bool operator==(const RuledHeadingRuleExtents&) const noexcept = default;
	};

	struct RowLeadingSlotRect
	{
		float minX{ 0.0f };
		float minY{ 0.0f };
		float maxX{ 0.0f };
		float maxY{ 0.0f };

		[[nodiscard]] constexpr float GetCenterY() const noexcept
		{
			return (minY + maxY) * 0.5f;
		}

		constexpr bool operator==(const RowLeadingSlotRect&) const noexcept = default;
	};

	[[nodiscard]] constexpr RowLeadingSlotRect ResolveRowLeadingSlotRect(
		float a_leadingMinX,
		float a_rowMinY,
		float a_rowMaxY,
		float a_slotSize) noexcept
	{
		const auto rowHeight = (std::max)(a_rowMaxY - a_rowMinY, 0.0f);
		const auto slotSize = (std::max)(a_slotSize, 0.0f);
		const auto minY = a_rowMinY + RowContentOffsetY(
			rowHeight,
			{ slotSize },
			RowContentMetric::kBox);
		return {
			a_leadingMinX,
			minY,
			a_leadingMinX + slotSize,
			minY + slotSize
		};
	}

	[[nodiscard]] constexpr RuledHeadingRuleExtents
		ResolveRuledHeadingRuleExtents(
			float a_outerMinX,
			float a_outerMaxX,
			float a_contentMinX,
			float a_contentMaxX,
			float a_contentGap) noexcept
	{
		const auto outerMaxX = (std::max)(a_outerMaxX, a_outerMinX);
		const auto contentMinX = std::clamp(
			a_contentMinX,
			a_outerMinX,
			outerMaxX);
		const auto contentMaxX = std::clamp(
			(std::max)(a_contentMaxX, contentMinX),
			contentMinX,
			outerMaxX);
		const auto contentGap = (std::max)(a_contentGap, 0.0f);
		return {
			{
				a_outerMinX,
				(std::max)(contentMinX - contentGap, a_outerMinX)
			},
			{
				(std::min)(contentMaxX + contentGap, outerMaxX),
				outerMaxX
			}
		};
	}
}
