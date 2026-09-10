#include <DearModdingUI/controls/LinkRow.h>

#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/VisualDecisions.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <string_view>
#include <vector>

namespace DearModdingUI
{
	DMUI_Result DrawLinkRow(
		const char* a_id,
		std::span<const LinkRowEntry> a_links) noexcept
	{
		if (a_links.empty())
			return DMUI_RESULT_OK;

		DMUI_Result result{ DMUI_RESULT_OK };
		const ExternalOpener opener;
		const auto& style = ImGui::GetStyle();
		const auto buttonWidth =
			(ImGui::GetContentRegionAvail().x -
				style.ItemSpacing.x *
					static_cast<float>(a_links.size() - 1)) /
			static_cast<float>(a_links.size());
		ImGui::PushID(a_id);
		for (size_t index = 0; index < a_links.size(); ++index)
		{
			const auto& link = a_links[index];
			ImGui::PushID(static_cast<int>(index));
			ImGui::BeginDisabled(!link.enabled);
			const auto clicked =
				ImGui::Button("##DearModdingUI.Link", { buttonWidth, 0.0f });
			const ImRect bounds{
				ImGui::GetItemRectMin(),
				ImGui::GetItemRectMax()
			};
			const auto textSize = ImGui::CalcTextSize(link.label.data());
			const auto layout = DecideInlineIconLayout(
				HasIconGlyph(link.glyph),
				textSize.x,
				textSize.y,
				ImGui::GetFontSize(),
				style.ItemSpacing.x);
			const ImVec4 clip{
				bounds.Min.x,
				bounds.Min.y,
				bounds.Max.x,
				bounds.Max.y
			};
			(void)DrawIconText(
				{
					bounds.GetCenter().x - layout.contentWidth * 0.5f,
					bounds.Min.y
				},
				bounds.GetHeight(),
				link.glyph,
				link.label.data(),
				ImGui::GetColorU32(ImGuiCol_Text),
				&clip);
			ImGui::EndDisabled();

			if (link.enabled && clicked)
			{
				if (link.action == DMUI_LINK_ACTION_COPY_TARGET)
					ImGui::SetClipboardText(link.external.target.c_str());
				else
				{
					DMUI_ExternalOpenDescriptor descriptor{
						sizeof(DMUI_ExternalOpenDescriptor),
						link.external.targetKind,
						link.external.target.empty() ?
							nullptr :
							link.external.target.c_str(),
						link.external.application.empty() ?
							nullptr :
							link.external.application.c_str(),
						nullptr,
						static_cast<uint32_t>(link.external.arguments.size()),
						0,
						link.external.workingDirectory.empty() ?
							nullptr :
							link.external.workingDirectory.c_str()
					};
					std::vector<const char*> arguments;
					arguments.reserve(link.external.arguments.size());
					for (const auto& argument : link.external.arguments)
						arguments.push_back(argument.c_str());
					descriptor.arguments =
						arguments.empty() ? nullptr : arguments.data();
					result = opener.Open(&descriptor);
				}
			}

			const auto hoverFlags =
				ImGuiHoveredFlags_DelayNormal |
				(link.enabled ?
						ImGuiHoveredFlags_None :
						ImGuiHoveredFlags_AllowWhenDisabled);
			if (ImGui::IsItemHovered(hoverFlags) && ImGui::BeginTooltip())
			{
				if (link.action == DMUI_LINK_ACTION_COPY_TARGET)
					ImGui::TextUnformatted("Copy target");
				else
				{
					if (link.external.targetKind ==
						DMUI_EXTERNAL_TARGET_VIRTUAL_FILE)
						ImGui::TextUnformatted("Open physical backing file");
					else if (link.external.targetKind ==
						DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT)
						ImGui::TextUnformatted(
							"Open physical containing folder");
					ImGui::TextUnformatted(
						link.external.application.empty() ?
							"Open with system default" :
							"Open with selected application");
				}
				const auto detail = link.note.empty() ?
					std::string_view{ link.external.target } :
					link.note;
				if (!detail.empty())
				ImGui::TextUnformatted(
					detail.data(),
					detail.data() + detail.size());
				ImGui::EndTooltip();
			}
			ImGui::PopID();
			if (index + 1 < a_links.size())
				ImGui::SameLine();
		}
		ImGui::PopID();
		return result;
	}
}
