#pragma once

#include <DearModdingUI/MCM/DiagnosticReporter.h>

#include <string>

namespace DearModdingUI::MCM
{
	class RexDiagnosticReporter final : public DiagnosticReporter
	{
	public:
		RexDiagnosticReporter(
			std::string a_mod,
			std::string a_clientId);

		void Report(Diagnostic a_diagnostic) noexcept override;

	private:
		std::string mod_;
		std::string clientId_;
	};
}
