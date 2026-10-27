#pragma once

#include <DearModdingUI/Button.h>
#include <DearModdingUI/API.h>

#include <imgui/imgui.h>

namespace DearModdingUI
{
	// The host button primitive: it owns the item, input, tooltip, and result,
	// and draws through the control renderer.
	struct ButtonSpec
	{
		// Item id; the label is used when null or empty.
		const char* id{};
		const char* label{};
		char32_t glyph{};
		const char* tooltip{};
		bool enabled{ true };
		dmui::ButtonStyle style{ dmui::ButtonStyle::kDefault };
		// Per axis: zero fits the content, negative fills the remaining region.
		ImVec2 size{};
		// Host chrome draws larger icons; zero uses the font size.
		float glyphSize{};
		// Shows a toggled-on look, such as the current page's entry.
		bool active{};
	};

	[[nodiscard]] DMUI_Result ValidateButton(
		const DMUI_ButtonDescriptor* a_button) noexcept;
	[[nodiscard]] ButtonSpec MakeButtonSpec(
		const DMUI_ButtonDescriptor& a_button) noexcept;
	// A glyph-only subtle button that falls back to its label without the icon.
	[[nodiscard]] ButtonSpec MakeIconButtonSpec(
		const char* a_id,
		char32_t a_glyph,
		const char* a_fallbackLabel,
		const char* a_tooltip) noexcept;

	// The size DrawButton takes at the current style.
	[[nodiscard]] ImVec2 MeasureButton(const ButtonSpec& a_button) noexcept;
	// Draws at the cursor and returns true on the frame it is pressed.
	[[nodiscard]] bool DrawButton(const ButtonSpec& a_button) noexcept;
	// Draws at a screen position, then restores the cursor and ends the line.
	[[nodiscard]] bool DrawButtonAt(
		const ButtonSpec& a_button,
		const ImVec2& a_origin) noexcept;
}
