#pragma once

#include <imgui/imgui.h>

namespace DearModdingUI::Theme
{
	struct LayoutStyle
	{
		float sectionGap{ 20.0f };
		ImVec2 panelPadding{ 12.0f, 12.0f };
	};

	[[nodiscard]] LayoutStyle& Layout() noexcept;
	void SectionSpacing() noexcept;
}
