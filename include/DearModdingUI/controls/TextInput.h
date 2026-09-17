#pragma once

#include <DearModdingUI/API.h>

namespace DearModdingUI
{
	[[nodiscard]] DMUI_Result DrawTextInput(
		const char* a_label,
		const char* a_hint,
		DMUI_TextBuffer& a_buffer,
		bool& a_changed) noexcept;
}
