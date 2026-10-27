#pragma once

#include <DearModdingUI/presentation/ThemeSpec.h>

#include <imgui/imgui.h>

namespace DearModdingUI
{
	struct LogoColors
	{
		ImU32 top;
		ImU32 left;
		ImU32 right;
		ImU32 bottom;
	};

	[[nodiscard]] LogoColors ResolveLogoColors(
		const Theme::ThemeSpec& a_theme) noexcept;

	void DrawLogo(
		ImDrawList* a_drawList,
		ImVec2 a_center,
		float a_radius,
		const LogoColors& a_colors) noexcept;
}
