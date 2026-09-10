#pragma once

#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/PageLookup.h>

namespace DearModdingUI::MCM
{
	inline void AttachTextRendering(MappedPage& a_page)
	{
		for (const auto& row : a_page.rows)
		{
			if (!row.text)
				continue;
			const auto& mapped = *row.text;
			auto* descriptor =
				FindSettingDescriptor(a_page.settings, mapped.descriptorId);
			if (!descriptor)
				continue;

			descriptor->control = dmui::ReadOnlySettingControl{
				[text = mapped.presentation.text,
				 alignment = mapped.presentation.alignment] {
					const auto availableWidth =
						dmui::ui::GetContentRegionAvail().x;
					const auto textWidth = dmui::ui::CalcTextSize(
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
						auto cursor = dmui::ui::GetCursorScreenPos();
						cursor.x += offset;
						dmui::ui::SetCursorScreenPos(cursor);
					}
					dmui::ui::TextWrapped("%s", text.c_str());
				}
			};
		}
	}
}
