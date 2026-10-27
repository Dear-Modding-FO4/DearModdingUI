#pragma once

#include <DearModdingUI/API.h>

namespace DearModdingUI
{
	[[nodiscard]] DMUI_Result ValidateSpinner(
		const DMUI_SpinnerDescriptor* a_spinner) noexcept;

	// The host busy indicator: it owns the item and draws through the control
	// renderer from the frame clock. Validate the descriptor first.
	void DrawSpinner(const DMUI_SpinnerDescriptor& a_spinner) noexcept;
}
