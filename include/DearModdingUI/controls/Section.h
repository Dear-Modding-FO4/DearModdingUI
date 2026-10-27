#pragma once

#include <DearModdingUI/controls/ControlRenderer.h>

#include <imgui/imgui.h>

#include <cstddef>
#include <optional>
#include <string_view>

namespace DearModdingUI
{
	enum class SectionLayout : uint32_t
	{
		// The title between two rules across the content region.
		kCentered,
		// An arrow row for the title with one rule after it.
		kLeadingRow
	};

	// The host section header: it owns the item, collapse input, and result, and
	// draws through the control renderer. The UI table's section brackets add the
	// ID scope and the optional box around the same header.
	struct SectionSpec
	{
		// Item id; the title is used when null or empty.
		const char* id{};
		const char* title{};
		// Zero infers an icon from glyphSource or a non-empty title; PhosphorGlyph::kNone draws none.
		char32_t glyph{};
		// Text the inferred icon derives from, so it stays the same in every language.
		const char* glyphSource{};
		std::optional<size_t> count;
		// Caller-owned collapse state; null is not collapsible.
		bool* expanded{};
		SectionLayout layout{ SectionLayout::kCentered };
		SectionRuleStyle ruleStyle{ SectionRuleStyle::kSection };
	};

	// Draws the header at the cursor; true while the content below should show.
	bool DrawSection(const SectionSpec& a_section) noexcept;
	// A plain header whose text is localized and whose icon derives from the English text.
	void DrawLocalizedSection(
		std::string_view a_key,
		const char* a_english) noexcept;
}
