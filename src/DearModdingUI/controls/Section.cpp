#include <DearModdingUI/controls/Section.h>

#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/localization/Localization.h>
#include <DearModdingUI/presentation/IconResolver.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <format>
#include <string>

namespace DearModdingUI
{
	namespace
	{
		// The icon comes from the English text so it stays the same in every language.
		[[nodiscard]] char32_t ResolveSectionGlyph(
			const SectionSpec& a_section,
			const char* a_title) noexcept
		{
			if (a_section.glyph == dmui::PhosphorGlyph::kNone)
				return 0;
			if (a_section.glyph != 0 || a_title[0] == '\0')
				return a_section.glyph;
			const auto source = a_section.glyphSource ?
				a_section.glyphSource :
				a_title;
			return dmui::ResolveIconSelection({}, source)
				.GlyphOr(dmui::PhosphorGlyph::kQuestion);
		}

		// The header toggles its caller-owned state; returns whether it is hovered.
		[[nodiscard]] bool DrawToggleItem(
			bool& a_expanded,
			const char* a_id,
			const ImVec2& a_size) noexcept
		{
			ImGui::PushID(a_id);
			if (ImGui::InvisibleButton(
					"##DearModdingUI.SectionHeader",
					a_size,
					ImGuiButtonFlags_EnableNav))
				a_expanded = !a_expanded;
			const auto hovered = ImGui::IsItemHovered();
			ImGui::PopID();
			return hovered;
		}
	}

	bool DrawSection(const SectionSpec& a_section) noexcept
	{
		const auto* title = a_section.title ? a_section.title : "";
		std::string counted;
		const auto* text = title;
		if (a_section.count)
		{
			try
			{
				counted = std::format("{} ({})", title, *a_section.count);
				text = counted.c_str();
			}
			catch (...)
			{}
		}
		const auto* id = a_section.id && a_section.id[0] != '\0' ?
			a_section.id :
			title;
		const auto glyph = ResolveSectionGlyph(a_section, title);
		const auto& renderer = CurrentControlRenderer();

		SectionHeaderContent content{
			text,
			glyph,
			a_section.expanded && !*a_section.expanded,
			a_section.ruleStyle
		};
		if (a_section.layout == SectionLayout::kLeadingRow)
		{
			const auto palette = renderer.SectionColors(content.minimized);
			const auto row = DrawSelectableRow({
				.id = id,
				.label = text,
				.leadingAffordance = RowLeadingAffordance::kArrow,
				.expanded = a_section.expanded,
				.glyph = glyph,
				.textColor = ImGui::GetColorU32(palette.text),
				.hoveredTextColor = ImGui::GetColorU32(palette.hovered),
				.highlightStyle = RowHighlightStyle::kRoundedFill
			});
			const auto& style = ImGui::GetStyle();
			const auto textMin = row.rect.Min.x + style.FramePadding.x;
			const auto textMax = (std::min)(
				row.rect.Min.x + style.FramePadding.x +
					ImGui::GetFontSize() * 2.0f +
					style.ItemInnerSpacing.x * 2.0f +
					ImGui::CalcTextSize(text).x,
				row.rect.Max.x);
			renderer.DrawSectionRules(
				row.rect, textMin, textMax, content, ImGui::IsItemHovered());
			return !a_section.expanded || *a_section.expanded;
		}

		const auto position = ImGui::GetCursorScreenPos();
		const auto width = ImGui::GetContentRegionAvail().x;
		const auto height = renderer.MeasureSectionHeader(content).height;
		auto hovered = false;
		if (a_section.expanded)
		{
			hovered = DrawToggleItem(*a_section.expanded, id, { width, height });
			content.minimized = !*a_section.expanded;
		}
		else
			ImGui::Dummy({ width, height });
		renderer.DrawSectionHeader(position, width, content, hovered);
		return !a_section.expanded || *a_section.expanded;
	}

	void DrawLocalizedSection(
		std::string_view a_key,
		const char* a_english) noexcept
	{
		DrawSection({
			.title = Localization::Text(a_key, a_english),
			.glyphSource = a_english
		});
	}
}
