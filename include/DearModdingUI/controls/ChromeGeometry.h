#pragma once

#include <DearModdingUI/VisualDecisions.h>

namespace DearModdingUI
{
	[[nodiscard]] constexpr float ResolveTitleBarButtonPadding(
		float a_framePaddingY) noexcept
	{
		return (a_framePaddingY > 0.0f ? a_framePaddingY : 0.0f) * 0.5f;
	}

	[[nodiscard]] constexpr float TitleBarButtonExtent(
		float a_fontSize,
		float a_buttonPadding) noexcept
	{
		const auto fontSize = a_fontSize > 0.0f ? a_fontSize : 0.0f;
		const auto padding = a_buttonPadding > 0.0f ? a_buttonPadding : 0.0f;
		return fontSize + padding * 2.0f;
	}

	inline constexpr float kHostChromeIconScale{ 1.5f };

	[[nodiscard]] constexpr float HostChromeIconSize(float a_fontSize) noexcept
	{
		return (a_fontSize > 0.0f ? a_fontSize : 0.0f) * kHostChromeIconScale;
	}

	[[nodiscard]] constexpr float HostChromeButtonExtent(
		float a_fontSize,
		float a_buttonPadding) noexcept
	{
		return TitleBarButtonExtent(HostChromeIconSize(a_fontSize), a_buttonPadding);
	}

	enum class TitleRowButtonExtentPolicy : uint32_t
	{
		kTitleBar,
		kHostChrome
	};

	[[nodiscard]] constexpr float ResolveTitleRowButtonExtent(
		TitleRowButtonExtentPolicy a_policy,
		float a_fontSize,
		float a_buttonPadding) noexcept
	{
		return a_policy == TitleRowButtonExtentPolicy::kHostChrome ?
			HostChromeButtonExtent(a_fontSize, a_buttonPadding) :
			TitleBarButtonExtent(a_fontSize, a_buttonPadding);
	}

	[[nodiscard]] constexpr float RightTitleBarButtonOriginX(
		float a_windowMaxX,
		float a_windowBorder,
		float a_framePaddingX,
		float a_fontSize,
		float a_offset,
		float a_buttonPadding) noexcept
	{
		return a_windowMaxX - a_windowBorder - a_framePaddingX -
			a_fontSize - a_offset - a_buttonPadding;
	}

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
