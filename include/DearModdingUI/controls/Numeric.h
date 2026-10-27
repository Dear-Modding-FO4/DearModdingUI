#pragma once

#include <DearModdingUI/API.h>

#include <optional>
#include <string>

namespace DearModdingUI
{
	[[nodiscard]] DMUI_Result ValidateNumeric(
		const DMUI_NumericDescriptor* a_numeric) noexcept;

	// Rebuilds a printf-style format holding one conversion for the argument
	// width the native widget formats, and appends the escaped unit. Empty when
	// the format is malformed or does not fit the type.
	[[nodiscard]] std::optional<std::string> BuildNumericFormat(
		const char* a_format,
		const char* a_unit,
		DMUI_NumericType a_type) noexcept;

	// The host numeric input: it owns ids, typing, bounds, stepping, and the
	// result, and draws through the control renderer. Validate the descriptor
	// first.
	[[nodiscard]] DMUI_NumericResult DrawNumeric(
		const DMUI_NumericDescriptor& a_numeric) noexcept;
}
