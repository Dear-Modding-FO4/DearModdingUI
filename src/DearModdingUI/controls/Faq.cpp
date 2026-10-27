#include <DearModdingUI/controls/Faq.h>

#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/controls/TextSpans.h>
#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>

namespace DearModdingUI
{
	void DrawFaq(
		const char* a_id,
		std::span<const FaqRowEntry> a_entries) noexcept
	{
		if (a_entries.empty())
			return;

		const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
		ImGui::PushID(a_id);
		ImGui::Indent();
		for (size_t index = 0; index < a_entries.size(); ++index)
		{
			const auto& entry = a_entries[index];
			ImGui::PushID(static_cast<int>(index));
			{
				const Theme::FontGuard font{ Theme::FontRole::kBody };
				(void)DrawSelectableRow({
					.id = "##DearModdingUI.FaqEntry",
					.label = entry.question.data(),
					.leadingAffordance = RowLeadingAffordance::kArrow,
					.expanded = entry.expanded,
					.textColor = textColor,
					.hoveredTextColor = textColor,
					.highlightStyle = RowHighlightStyle::kRoundedFill
				});
			}
			if (*entry.expanded)
			{
				ImGui::Indent();
				(void)DrawTextSpans(
					entry.answer,
					dmui::TextStyle{}.Role(DMUI_FONT_ROLE_SUBTEXT),
					{ .wrap = true });
				ImGui::Unindent();
				ImGui::Spacing();
			}
			ImGui::PopID();
		}
		ImGui::Unindent();
		ImGui::PopID();
	}
}
