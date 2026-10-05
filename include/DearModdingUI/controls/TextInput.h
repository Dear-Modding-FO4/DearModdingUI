#pragma once

#include <DearModdingUI/CUIAPI.h>

#include <imgui/imgui.h>

namespace DearModdingUI
{
	[[nodiscard]] DMUI_Result DrawTextInput(
		const char* a_label,
		const char* a_hint,
		DMUI_TextBuffer& a_buffer,
		bool& a_changed) noexcept;

	// Single-line editor over the same editing core, reporting DMUI_UITextEditEvents.
	[[nodiscard]] DMUI_Result DrawTextEditor(
		const char* a_label,
		const char* a_hint,
		DMUI_TextBuffer& a_buffer,
		ImGuiInputTextFlags a_flags,
		DMUI_UITextEditFlags a_editFlags,
		size_t a_cursor,
		DMUI_TextEditState& a_state) noexcept;
}
