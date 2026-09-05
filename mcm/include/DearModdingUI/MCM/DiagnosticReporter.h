#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

namespace DearModdingUI::MCM
{
	class DiagnosticReporter
	{
	public:
		DiagnosticReporter() = default;
		virtual ~DiagnosticReporter() = default;

		DiagnosticReporter(const DiagnosticReporter&) = delete;
		DiagnosticReporter(DiagnosticReporter&&) = delete;
		DiagnosticReporter& operator=(const DiagnosticReporter&) = delete;
		DiagnosticReporter& operator=(DiagnosticReporter&&) = delete;

		virtual void Report(Diagnostic a_diagnostic) noexcept = 0;
	};
}
