#pragma once

#include <DearModdingUI/API.h>

#include <imgui/imgui.h>

#include <cstdint>

namespace DearModdingUI
{
	inline constexpr uint32_t kMaxColorPresets{ 64 };

	// Converts between packed 0xRRGGBBAA and the float color native widgets edit.
	[[nodiscard]] ImVec4 UnpackRgba(uint32_t a_rgba) noexcept;
	[[nodiscard]] uint32_t PackRgba(const ImVec4& a_color) noexcept;

	[[nodiscard]] DMUI_Result ValidateColorEdit(
		const DMUI_ColorEditDescriptor* a_color) noexcept;

	// The host color edit: it owns ids, the preset layout, and the result, and
	// draws through the control renderer. Validate the descriptor first.
	[[nodiscard]] DMUI_ColorEditResult DrawColorEdit(
		const DMUI_ColorEditDescriptor& a_color) noexcept;
}
