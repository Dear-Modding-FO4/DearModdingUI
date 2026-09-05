#include <DearModdingUI/MCM/RexDiagnosticReporter.h>

#include <REX/REX.h>

namespace DearModdingUI::MCM
{
	using namespace std::literals;

	RexDiagnosticReporter::RexDiagnosticReporter(
		std::string a_mod,
		std::string a_clientId) :
		mod_(std::move(a_mod)),
		clientId_(std::move(a_clientId))
	{}

	void RexDiagnosticReporter::Report(Diagnostic a_diagnostic) noexcept
	{
		if (a_diagnostic.location.empty())
		{
			if (a_diagnostic.severity == DiagnosticSeverity::kWarning)
				REX::WARN("[dmui.mcm.diagnostic] mod=\"{}\" client_id=\"{}\" {}: {}"sv,
					mod_, clientId_, a_diagnostic.source, a_diagnostic.message);
			else
				REX::ERROR("[dmui.mcm.diagnostic] mod=\"{}\" client_id=\"{}\" {}: {}"sv,
					mod_, clientId_, a_diagnostic.source, a_diagnostic.message);
			return;
		}
		if (a_diagnostic.severity == DiagnosticSeverity::kWarning)
			REX::WARN("[dmui.mcm.diagnostic] mod=\"{}\" client_id=\"{}\" {}: {}: {}"sv,
				mod_, clientId_, a_diagnostic.source, a_diagnostic.location,
				a_diagnostic.message);
		else
			REX::ERROR("[dmui.mcm.diagnostic] mod=\"{}\" client_id=\"{}\" {}: {}: {}"sv,
				mod_, clientId_, a_diagnostic.source, a_diagnostic.location,
				a_diagnostic.message);
	}
}
