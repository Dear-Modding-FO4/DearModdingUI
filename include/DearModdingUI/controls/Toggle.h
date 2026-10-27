#pragma once

#include <DearModdingUI/API.h>

namespace DearModdingUI
{
	[[nodiscard]] DMUI_Result ValidateToggle(
		const DMUI_ToggleDescriptor* a_toggle) noexcept;

	// The host checkbox: it owns the id and result, and draws through the
	// control renderer. Validate the descriptor first.
	[[nodiscard]] DMUI_ToggleResult DrawToggle(
		const DMUI_ToggleDescriptor& a_toggle) noexcept;
}
