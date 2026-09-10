#include <DearModdingUI/controls/Faq.h>

#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>

#include <map>
#include <string>

namespace DearModdingUI
{
	void DrawFaq(
		const char* a_id,
		std::span<const FaqRowEntry> a_entries) noexcept
	{
		if (a_entries.empty())
			return;

		static std::map<std::string, bool> expansion;
		const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
		ImGui::PushID(a_id);
		ImGui::Indent();
		for (size_t index = 0; index < a_entries.size(); ++index)
		{
			const auto& entry = a_entries[index];
			auto expanded = expansion.try_emplace(
				BuildFaqExpansionKey(a_id, index),
				false).first;
			ImGui::PushID(static_cast<int>(index));
			{
				const Theme::FontGuard font{ Theme::FontRole::kBody };
				(void)DrawSelectableRow({
					.id = "##DearModdingUI.FaqEntry",
					.label = entry.question.data(),
					.leadingAffordance = RowLeadingAffordance::kArrow,
					.expanded = &expanded->second,
					.textColor = textColor,
					.hoveredTextColor = textColor,
					.highlightStyle = RowHighlightStyle::kRoundedFill,
					.clickBehavior = RowClickBehavior::kToggle
				});
			}
			if (expanded->second)
			{
				const Theme::FontGuard font{ Theme::FontRole::kSubtext };
				ImGui::Indent();
				ImGui::TextWrapped(
					"%.*s",
					static_cast<int>(entry.answer.size()),
					entry.answer.data());
				ImGui::Unindent();
				ImGui::Spacing();
			}
			ImGui::PopID();
		}
		ImGui::Unindent();
		ImGui::PopID();
	}
}
