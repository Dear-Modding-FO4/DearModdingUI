#pragma once

#include <DearModdingUI/MCM/DiagnosticReporter.h>

#include <vector>

namespace vmm_tests
{
	class FakeDiagnosticReporter final :
		public DearModdingUI::MCM::DiagnosticReporter
	{
	public:
		void Report(
			DearModdingUI::MCM::Diagnostic a_diagnostic) noexcept override
		{
			diagnostics.push_back(std::move(a_diagnostic));
		}

		std::vector<DearModdingUI::MCM::Diagnostic> diagnostics;
	};
}
