#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/controls/Button.h>
#include <DearModdingUI/controls/ChromeGeometry.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/presentation/VisualDecisions.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace DearModdingUI
{
	enum class RowLeadingAffordance : uint32_t
	{
		kNone,
		kArrow,
		kBack,
		kIcon
	};

	enum class RowHighlightStyle : uint32_t
	{
		kSelectable,
		kRoundedFill
	};

	enum class RowDisclosureStyle : uint32_t
	{
		kDefault,
		kSubtle
	};

	struct RowOptions
	{
		const char* id{ nullptr };
		const char* label{ nullptr };
		bool selected{ false };
		float height{ 0.0f };
		RowLeadingAffordance leadingAffordance{
			RowLeadingAffordance::kNone
		};
		bool* expanded{ nullptr };
		char32_t glyph{ 0 };
		ImU32 textColor{ 0 };
		ImU32 hoveredTextColor{ 0 };
		std::optional<ImU32> disabledColor;
		float trailingWidth{ 0.0f };
		bool flushHorizontalHighlight{ false };
		RowHighlightStyle highlightStyle{
			RowHighlightStyle::kSelectable
		};
		bool centerGlyph{ false };
		RowDisclosureStyle disclosureStyle{ RowDisclosureStyle::kDefault };
	};

	struct RowResult
	{
		bool pressed{ false };
		ImRect rect;
	};

	struct TitleRowOptions
	{
		const char* title;
		Theme::FontRole titleFont{ Theme::FontRole::kTitle };
		float titleScale{ 1.0f };
		std::span<const ButtonSpec> buttons;
		// Draws button icons at this size; zero uses the font size.
		float buttonGlyphSize{ 0.0f };
		bool drawSeparator{ true };
		const char* summary{};
		void (*drawLeadingMark)(ImDrawList*, ImVec2, float) noexcept{};
	};

	void PlaceRowContent(const ImVec2& a_position) noexcept;
	[[nodiscard]] bool HasIconGlyph(char32_t a_glyph) noexcept;
	[[nodiscard]] ImU32 IconColor(
		ImU32 a_textColor,
		float a_alpha = 1.0f) noexcept;
	void DrawCenteredIcon(
		ImDrawList* a_drawList,
		char32_t a_glyph,
		const ImRect& a_bounds,
		float a_size,
		ImU32 a_color,
		const ImVec4* a_clip = nullptr) noexcept;
	[[nodiscard]] float DrawIconText(
		const ImVec2& a_position,
		float a_height,
		char32_t a_glyph,
		const char* a_text,
		ImU32 a_color,
		const ImVec4* a_clip = nullptr,
		float a_iconSize = 0.0f) noexcept;
	// Fills a rounded hover or press highlight; false when neither applies.
	[[nodiscard]] bool DrawRoundedHighlight(
		const ImVec2& a_min,
		const ImVec2& a_max,
		bool a_hovered,
		bool a_active,
		ImDrawList* a_drawList) noexcept;
	[[nodiscard]] float TitleBarButtonPadding() noexcept;
	[[nodiscard]] float SeparatorThickness() noexcept;
	[[nodiscard]] bool BeginWithRoundedTitleBarButtons(
		const char* a_name,
		bool* a_open,
		ImGuiWindowFlags a_flags) noexcept;
	[[nodiscard]] bool BeginPopupModalWithRoundedTitleBarButtons(
		const char* a_name,
		bool* a_open,
		ImGuiWindowFlags a_flags) noexcept;
	void SetNextWrappedModalWidth() noexcept;
	[[nodiscard]] RowResult DrawSelectableRow(
		const RowOptions& a_options) noexcept;
	[[nodiscard]] std::optional<size_t> DrawTitleRow(
		const TitleRowOptions& a_options) noexcept;
}
