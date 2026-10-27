#pragma once

#include <DearModdingUI/API.h>

namespace DearModdingUI
{
	[[nodiscard]] DMUI_Result ValidateChoice(
		const DMUI_ChoiceDescriptor* a_choice) noexcept;

	// The host dropdown: it owns ids, input, option tooltips, and the result, and
	// draws through the control renderer. Validate the descriptor first.
	[[nodiscard]] DMUI_ChoiceResult DrawChoice(
		const DMUI_ChoiceDescriptor& a_choice) noexcept;
}
