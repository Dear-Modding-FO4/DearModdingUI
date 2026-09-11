#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/controls/ChromeGeometry.h>
#include <DearModdingUI/SettingsActions.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/VisualDecisions.h>

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

	enum class RowClickBehavior : uint32_t
	{
		kSelect,
		kToggle
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
		RowClickBehavior clickBehavior{ RowClickBehavior::kSelect };
		bool centerGlyph{ false };
		RowDisclosureStyle disclosureStyle{ RowDisclosureStyle::kDefault };
	};

	struct RowResult
	{
		bool pressed{ false };
		ImRect rect;
	};

	enum class RuledHeadingLayout : uint32_t
	{
		kCentered,
		kLeadingRow
	};

	enum class RuledHeadingRuleStyle : uint32_t
	{
		kSection,
		kSubordinate
	};

	struct RuledHeadingOptions
	{
		const char* key{ nullptr };
		const char* text{ nullptr };
		char32_t glyph{ 0 };
		std::optional<size_t> count;
		bool* expanded{ nullptr };
		RuledHeadingLayout layout{ RuledHeadingLayout::kCentered };
		RuledHeadingRuleStyle ruleStyle{
			RuledHeadingRuleStyle::kSection
		};
	};

	struct TitleRowButton
	{
		const char* id;
		float width;
		char32_t glyph;
		const char* fallbackLabel;
		const char* tooltip;
		bool enabled{ true };
		bool active{ false };
	};

	struct TitleRowOptions
	{
		const char* title;
		Theme::FontRole titleFont{ Theme::FontRole::kTitle };
		float titleScale{ 1.0f };
		float titleInsetX{};
		std::span<const TitleRowButton> buttons;
		TitleRowButtonExtentPolicy buttonExtentPolicy{
			TitleRowButtonExtentPolicy::kTitleBar
		};
		bool drawSeparator{ true };
		const char* summary{};
	};

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
		const ImVec4* a_clip = nullptr) noexcept;
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
	[[nodiscard]] bool DrawCompactChromeButton(
		const char* a_id,
		const ImVec2& a_origin,
		const ImVec2& a_size,
		char32_t a_glyph,
		const char* a_text,
		const char* a_tooltip,
		ImU32 a_color,
		bool a_active = false,
		float a_glyphSize = 0.0f) noexcept;
	[[nodiscard]] RowResult DrawSelectableRow(
		const RowOptions& a_options) noexcept;
	void DrawRuledHeading(const RuledHeadingOptions& a_options) noexcept;
	[[nodiscard]] std::optional<size_t> DrawTitleRow(
		const TitleRowOptions& a_options) noexcept;

	[[nodiscard]] float SettingsActionButtonExtent() noexcept;
	[[nodiscard]] float SettingsActionButtonWidth(
		SettingsAction a_action,
		const char* a_fallbackLabel,
		float a_buttonExtent) noexcept;
	[[nodiscard]] bool DrawSettingsActionButton(
		const char* a_id,
		const ImVec2& a_origin,
		const ImVec2& a_size,
		SettingsAction a_action,
		const char* a_fallbackLabel,
		const char* a_tooltip,
		bool a_enabled) noexcept;
	void DrawSearchInput(
		const char* a_id,
		const char* a_hint,
		std::string& a_search) noexcept;
	void DrawSectionHeader(const char* a_text, char32_t a_glyph) noexcept;
	void DrawSectionHeader(const char* a_text);
	void DrawBulletText(const char* a_text) noexcept;
	void DrawCollapsingSectionHeader(
		const char* a_key,
		const char* a_text,
		char32_t a_glyph,
		bool& a_expanded,
		size_t a_count) noexcept;
}
