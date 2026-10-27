#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace DearModdingUI
{
	struct InlineIconLayout
	{
		bool drawIcon{ false };
		float iconSize{ 0.0f };
		float textOffset{ 0.0f };
		float contentWidth{ 0.0f };
		float contentHeight{ 0.0f };
	};

	[[nodiscard]] constexpr InlineIconLayout DecideInlineIconLayout(
		bool a_hasIcon,
		float a_textWidth,
		float a_textHeight,
		float a_fontSize,
		float a_spacing) noexcept
	{
		const auto textWidth = a_textWidth > 0.0f ? a_textWidth : 0.0f;
		const auto textHeight = a_textHeight > 0.0f ? a_textHeight : 0.0f;
		const auto iconSize = a_hasIcon && a_fontSize > 0.0f ? a_fontSize : 0.0f;
		const auto spacing = iconSize > 0.0f && a_spacing > 0.0f ? a_spacing : 0.0f;
		return {
			iconSize > 0.0f,
			iconSize,
			iconSize + spacing,
			textWidth + iconSize + spacing,
			textHeight > iconSize ? textHeight : iconSize
		};
	}

	struct GlyphDrawOrigin
	{
		float x{ 0.0f };
		float y{ 0.0f };

		constexpr bool operator==(const GlyphDrawOrigin&) const noexcept = default;
	};

	[[nodiscard]] constexpr GlyphDrawOrigin ResolveCenteredGlyphOrigin(
		float a_centerX,
		float a_centerY,
		float a_glyphMinX,
		float a_glyphMinY,
		float a_glyphMaxX,
		float a_glyphMaxY,
		float a_scale) noexcept
	{
		const auto scale = a_scale > 0.0f ? a_scale : 0.0f;
		return {
			a_centerX - (a_glyphMinX + a_glyphMaxX) * scale * 0.5f,
			a_centerY - (a_glyphMinY + a_glyphMaxY) * scale * 0.5f
		};
	}

	// Leftmost bullet ink: ImGui centers it at FramePadding.x + FontSize/2 with radius FontSize/5.
	[[nodiscard]] constexpr float BulletRunContentInset(
		float a_framePaddingX,
		float a_fontSize) noexcept
	{
		const auto framePaddingX = a_framePaddingX > 0.0f ?
			a_framePaddingX :
			0.0f;
		const auto fontSize = a_fontSize > 0.0f ? a_fontSize : 0.0f;
		return framePaddingX + fontSize * 0.3f;
	}

	enum class RowContentRectKind : uint32_t
	{
		kSelectable,
		kTable
	};

	struct RowContentRect
	{
		float minX{ 0.0f };
		float minY{ 0.0f };
		float maxX{ 0.0f };
		float maxY{ 0.0f };

		[[nodiscard]] constexpr float GetWidth() const noexcept
		{
			return (std::max)(maxX - minX, 0.0f);
		}

		[[nodiscard]] constexpr float GetHeight() const noexcept
		{
			return (std::max)(maxY - minY, 0.0f);
		}

		constexpr bool operator==(const RowContentRect&) const noexcept = default;
	};

	struct SelectableRowContentRectProvider
	{
		inline static constexpr auto kind = RowContentRectKind::kSelectable;

		[[nodiscard]] static constexpr RowContentRect Resolve(
			const RowContentRect& a_containerRect,
			[[maybe_unused]] float a_rowCellPaddingY) noexcept
		{
			return a_containerRect;
		}
	};

	struct TableRowContentRectProvider
	{
		inline static constexpr auto kind = RowContentRectKind::kTable;

		[[nodiscard]] static constexpr RowContentRect Resolve(
			const RowContentRect& a_containerRect,
			float a_rowCellPaddingY) noexcept
		{
			const auto padding = (std::max)(a_rowCellPaddingY, 0.0f);
			auto result = a_containerRect;
			result.minY = (std::min)(
				a_containerRect.minY + padding,
				a_containerRect.maxY);
			result.maxY = (std::max)(
				a_containerRect.maxY - padding,
				result.minY);
			return result;
		}
	};

	template<class F>
	constexpr decltype(auto) VisitRowContentRectProvider(
		RowContentRectKind a_kind,
		F&& a_fn)
	{
		if (a_kind == RowContentRectKind::kSelectable)
			return a_fn.template operator()<SelectableRowContentRectProvider>();
		return a_fn.template operator()<TableRowContentRectProvider>();
	}

	[[nodiscard]] constexpr RowContentRect ResolveRowContentRect(
		RowContentRectKind a_kind,
		const RowContentRect& a_containerRect,
		float a_rowCellPaddingY = 0.0f) noexcept
	{
		return VisitRowContentRectProvider(
			a_kind,
			[&]<class Provider>() {
				return Provider::Resolve(
					a_containerRect,
					a_rowCellPaddingY);
			});
	}

	struct RowContentLayout
	{
		float leadingMinX{ 0.0f };
		float iconMinX{ 0.0f };
		float textMinX{ 0.0f };
		float clipMaxX{ 0.0f };

		constexpr bool operator==(const RowContentLayout&) const noexcept = default;
	};

	[[nodiscard]] constexpr RowContentLayout ResolveRowContentLayout(
		float a_rowMinX,
		float a_rowMaxX,
		float a_framePaddingX,
		float a_slotSize,
		float a_leadingSpacing,
		float a_iconSpacing,
		bool a_hasLeadingSlot,
		bool a_hasIconSlot,
		float a_trailingWidth) noexcept
	{
		const auto rowMaxX = (std::max)(a_rowMaxX, a_rowMinX);
		const auto framePaddingX = (std::max)(a_framePaddingX, 0.0f);
		const auto slotSize = (std::max)(a_slotSize, 0.0f);
		const auto leadingSpacing = (std::max)(a_leadingSpacing, 0.0f);
		const auto iconSpacing = (std::max)(a_iconSpacing, 0.0f);
		const auto trailingWidth = (std::max)(a_trailingWidth, 0.0f);
		auto cursor = a_rowMinX + framePaddingX;
		const auto leadingMinX = cursor;
		if (a_hasLeadingSlot)
			cursor += slotSize + leadingSpacing;
		const auto iconMinX = cursor;
		if (a_hasIconSlot)
			cursor += slotSize + iconSpacing;
		return {
			leadingMinX,
			iconMinX,
			cursor,
			(std::max)(rowMaxX - trailingWidth, a_rowMinX)
		};
	}

	struct TrailingControlLayout
	{
		float adjacentMinX{ 0.0f };
		float adjacentMaxX{ 0.0f };
		float controlMinX{ 0.0f };
		float controlMaxX{ 0.0f };
	};

	[[nodiscard]] constexpr TrailingControlLayout ResolveTrailingControlLayout(
		float a_contentMinX,
		float a_contentMaxX,
		float a_controlExtent,
		float a_spacing) noexcept
	{
		const auto contentMax = a_contentMaxX > a_contentMinX ?
			a_contentMaxX :
			a_contentMinX;
		const auto extent = a_controlExtent > 0.0f ? a_controlExtent : 0.0f;
		const auto spacing = a_spacing > 0.0f ? a_spacing : 0.0f;
		const auto unboundedControlMin = contentMax - extent;
		const auto controlMin = unboundedControlMin > a_contentMinX ?
			unboundedControlMin :
			a_contentMinX;
		const auto unboundedAdjacentMax = controlMin - spacing;
		const auto adjacentMax = unboundedAdjacentMax > a_contentMinX ?
			unboundedAdjacentMax :
			a_contentMinX;
		return {
			a_contentMinX,
			adjacentMax,
			controlMin,
			contentMax
		};
	}
}
