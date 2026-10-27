#pragma once

#include <DearModdingUI/CUIAPI.h>

#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace DearModdingUI
{
	// The host text field: the one editing core behind every text input. The
	// DMUI_TextFieldOptions comment defines each field; flags are translated.
	struct TextFieldSpec
	{
		const char* hint{};
		DMUI_TextFieldStyle style{ DMUI_TEXT_FIELD_STYLE_PLAIN };
		ImGuiInputTextFlags flags{};
		DMUI_UITextEditFlags editFlags{};
		uint32_t maxBytes{};
		uint32_t lines{};
		ImVec2 size{};
		size_t cursor{};
		// Single line only; see DMUI_TextStyleFn.
		DMUI_TextStyleFn styleSpans{};
		void* styleUserData{};
	};

	// Draws at the cursor and reports per-frame edit events in a_state. The id
	// scopes the field and is never drawn.
	[[nodiscard]] DMUI_Result DrawTextField(
		const char* a_id,
		DMUI_TextBuffer& a_buffer,
		const TextFieldSpec& a_spec,
		DMUI_TextEditState& a_state) noexcept;

	// Edits a string through a growable buffer; a_edited reports a change.
	[[nodiscard]] DMUI_Result DrawTextField(
		const char* a_id,
		std::string& a_text,
		const TextFieldSpec& a_spec,
		bool& a_edited) noexcept;
}
