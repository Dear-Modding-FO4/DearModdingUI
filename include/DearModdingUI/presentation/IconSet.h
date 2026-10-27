#pragma once

#include <DearModdingUI/presentation/IconResolver.h>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI::Theme
{
	struct IconMapping
	{
		// The canonical Phosphor codepoint hosts and clients draw by.
		char32_t phosphor;
		// The same icon's codepoint in the set's font.
		char32_t glyph;

		[[nodiscard]] bool operator==(const IconMapping&) const noexcept = default;
	};

	// Plain data: a font file plus a Phosphor-to-font glyph map.
	struct IconSet
	{
		// Relative to the Fonts root; empty keeps Phosphor everywhere.
		std::string font;
		// Sorted by phosphor with unique keys; build it with MapIconNames.
		std::vector<IconMapping> glyphs;
		// Fraction of the icon size the font's glyphs are shifted down.
		float baselineOffsetRatio{ 4.0f / 21.0f };

		[[nodiscard]] char32_t Find(char32_t a_phosphor) const noexcept
		{
			const auto found = std::ranges::lower_bound(
				glyphs,
				a_phosphor,
				{},
				&IconMapping::phosphor);
			return found != glyphs.end() && found->phosphor == a_phosphor ?
				found->glyph :
				char32_t{};
		}

		[[nodiscard]] bool operator==(const IconSet&) const noexcept = default;
	};

	struct NamedIconGlyph
	{
		std::string_view name;
		char32_t glyph;
	};

	struct IconMapResult
	{
		std::vector<IconMapping> glyphs;
		// Names that match no Phosphor icon; the caller decides how to report.
		std::vector<std::string> unknownNames;
	};

	// A repeated icon keeps its last entry.
	[[nodiscard]] inline IconMapResult MapIconNames(
		std::span<const NamedIconGlyph> a_named)
	{
		std::vector<IconMapping> all;
		IconMapResult result;
		for (const auto& entry : a_named)
		{
			if (const auto phosphor = dmui::ResolveNamedIconGlyphOrZero(entry.name))
				all.push_back({ phosphor, entry.glyph });
			else
				result.unknownNames.emplace_back(entry.name);
		}
		std::ranges::stable_sort(all, {}, &IconMapping::phosphor);
		for (const auto& mapping : all)
		{
			if (!result.glyphs.empty() &&
				result.glyphs.back().phosphor == mapping.phosphor)
				result.glyphs.back() = mapping;
			else
				result.glyphs.push_back(mapping);
		}
		return result;
	}
}
