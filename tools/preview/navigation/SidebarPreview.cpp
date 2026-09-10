#include "SidebarPreview.h"

#if defined(DMUI_PREVIEW)

#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>

#include <cfloat>

namespace DearModdingUI
{
	void DrawIconRailNavigation(
		const SidebarViewContext& a_context) noexcept
	{
		const auto& style = ImGui::GetStyle();
		float iconFontSize{};
		{
			const Theme::FontGuard font{
				Theme::FontRole::kTitle,
				0.8f
			};
			iconFontSize = ImGui::GetFontSize();
		}
		const auto geometry = ResolveIconRailGeometry(
			ImGui::GetContentRegionAvail().x,
			iconFontSize,
			style.FramePadding.x,
			style.ItemSpacing.x);
		if (geometry.railWidth > 0.0f)
		{
			if (ImGui::BeginChild(
					"##DearModdingIconRail",
					{ geometry.railWidth, -FLT_MIN }))
			{
				DrawPresentedSidebarClients(
					a_context,
					SidebarClientRowKind::kRail);
			}
			ImGui::EndChild();
		}
		if (geometry.panelWidth <= 0.0f)
			return;

		ImGui::SameLine(0.0f, geometry.gap);
		if (ImGui::BeginChild(
				"##DearModdingIconRailPages",
				{ geometry.panelWidth, -FLT_MIN }))
		{
			if (const auto* client =
					a_context.model.FindClient(
						a_context.selection.activeClient))
			{
				DrawSidebarClientContext(*client);
				ImGui::Spacing();
				DrawSectionHeader(
					client->displayName.c_str(),
					ResolveNavigationClientIconGlyph(*client));
				ImGui::Spacing();
				DrawSidebarPageList(a_context, *client);
			}
			else
				ImGui::TextDisabled("Select a mod to browse its pages.");
		}
		ImGui::EndChild();
	}
}

#endif
