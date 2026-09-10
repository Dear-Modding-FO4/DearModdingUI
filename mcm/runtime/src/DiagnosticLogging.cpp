#include <DearModdingUI/MCM/DiagnosticLogging.h>

#include <REX/REX.h>

namespace DearModdingUI::MCM
{
	void LogDiagnostic(
		const Diagnostic& a_diagnostic,
		DiagnosticLogContext a_context) noexcept
	{
		const auto separator = a_diagnostic.location.empty() ? "" : ": ";
		const auto warning = a_diagnostic.severity == DiagnosticSeverity::kWarning;
		if (!a_context.mod.empty() || !a_context.clientId.empty())
		{
			if (warning)
				REX::WARN("{} mod=\"{}\" client_id=\"{}\" {}: {}{}{}",
					kDiagnosticLogTag, a_context.mod, a_context.clientId,
					a_diagnostic.source, a_diagnostic.location, separator, a_diagnostic.message);
			else
				REX::ERROR("{} mod=\"{}\" client_id=\"{}\" {}: {}{}{}",
					kDiagnosticLogTag, a_context.mod, a_context.clientId,
					a_diagnostic.source, a_diagnostic.location, separator, a_diagnostic.message);
		}
		else
		{
			if (warning)
				REX::WARN("DearModdingUI-MCM: {}: {}{}{}",
					a_diagnostic.source, a_diagnostic.location, separator, a_diagnostic.message);
			else
				REX::ERROR("DearModdingUI-MCM: {}: {}{}{}",
					a_diagnostic.source, a_diagnostic.location, separator, a_diagnostic.message);
		}
	}
}
