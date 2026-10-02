#include <DearModdingUI/presentation/ThemeLayout.h>

namespace DearModdingUI::Theme
{
	LayoutStyle& Layout() noexcept
	{
		static LayoutStyle layout;
		return layout;
	}

	void SectionSpacing() noexcept
	{
		const auto spacing = ImGui::GetStyle().ItemSpacing;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - spacing.y);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ spacing.x, Layout().sectionGap });
		ImGui::Dummy({});
		ImGui::PopStyleVar();
	}
}
