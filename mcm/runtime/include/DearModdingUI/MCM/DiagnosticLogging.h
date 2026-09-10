#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <string_view>

namespace DearModdingUI::MCM
{
	inline constexpr std::string_view kDiagnosticLogTag{
		"[dmui.mcm.diagnostic]"
	};

	struct DiagnosticLogContext
	{
		std::string_view mod;
		std::string_view clientId;
	};

	void LogDiagnostic(
		const Diagnostic& a_diagnostic,
		DiagnosticLogContext a_context = {}) noexcept;
}
