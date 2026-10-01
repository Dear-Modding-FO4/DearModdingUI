#pragma once

#include <imgui/imgui.h>

namespace DearModdingUI
{
	struct LogoColors
	{
		ImU32 top{ IM_COL32(0x9E, 0x00, 0x5D, 0xFF) };
		ImU32 left{ IM_COL32(0x19, 0x7B, 0x30, 0xFF) };
		ImU32 right{ IM_COL32(0x00, 0x4A, 0x80, 0xFF) };
		ImU32 bottom{ IM_COL32(0xF7, 0x94, 0x1D, 0xFF) };
	};

	void DrawLogo(
		ImDrawList* a_drawList,
		ImVec2 a_center,
		float a_radius,
		const LogoColors& a_colors = {}) noexcept;
}
