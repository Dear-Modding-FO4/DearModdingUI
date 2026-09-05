#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

namespace DearModdingUI::MCM
{
	inline void AttachTextRendering(MappedPage& a_page)
	{
		for (const auto& row : a_page.rows)
		{
			if (!row.text)
				continue;
			const auto& mapped = *row.text;
			dmui::SettingDescriptor* descriptor{};
			for (auto& group : a_page.settings.groups)
			{
				for (auto& setting : group.settings)
				{
					if (setting.id == mapped.descriptorId)
					{
						descriptor = &setting;
						break;
					}
				}
				if (descriptor)
					break;
			}
			if (!descriptor)
				continue;

			descriptor->control = dmui::ReadOnlySettingControl{
				[text = mapped.presentation.text,
				 alignment = mapped.presentation.alignment] {
					const auto availableWidth =
						ImGui::GetContentRegionAvail().x;
					const auto textWidth = ImGui::CalcTextSize(
						text.c_str(),
						nullptr,
						false,
						availableWidth).x;
					auto offset = 0.0f;
					if (alignment == TextAlignment::kCenter)
						offset = (availableWidth - textWidth) * 0.5f;
					else if (alignment == TextAlignment::kRight)
						offset = availableWidth - textWidth;
					if (offset > 0.0f)
					{
						auto cursor = ImGui::GetCursorScreenPos();
						cursor.x += offset;
						ImGui::SetCursorScreenPos(cursor);
					}
					ImGui::TextWrapped("%s", text.c_str());
				}
			};
		}
	}
}
