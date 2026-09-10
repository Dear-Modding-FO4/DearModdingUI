#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/host/Diagnostics.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	void run_diagnostic_api_checks(Runner& runner)
	{
		runner.test("diagnostic API arguments reject malformed descriptors", [] {
			const DMUI_ReportDiagnosticFn reportDiagnostic =
				&ValidateDiagnosticArguments;
			DMUI_DiagnosticDescriptor diagnostic{
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
				DMUI_STATUS_SEVERITY_WARNING,
				nullptr,
				"Expected a boolean value.",
				nullptr
			};
			require(
				reportDiagnostic(
					DMUI_INVALID_CLIENT_HANDLE,
					nullptr) == DMUI_RESULT_INVALID_ARGUMENT,
				"a null diagnostic descriptor was accepted");
			diagnostic.summary = "";
			require(
				reportDiagnostic(
					DMUI_INVALID_CLIENT_HANDLE,
					&diagnostic) == DMUI_RESULT_INVALID_ARGUMENT,
				"an empty diagnostic summary was accepted");
			diagnostic.summary = "Expected a boolean value.";
			diagnostic.severity = 99;
			require(
				reportDiagnostic(
					DMUI_INVALID_CLIENT_HANDLE,
					&diagnostic) == DMUI_RESULT_INVALID_ARGUMENT,
				"an unknown diagnostic severity was accepted");
			diagnostic.severity = DMUI_STATUS_SEVERITY_WARNING;
			diagnostic.structSize =
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE - 1;
			require(
				reportDiagnostic(
					DMUI_INVALID_CLIENT_HANDLE,
					&diagnostic) == DMUI_RESULT_INVALID_ARGUMENT,
				"a short diagnostic descriptor was accepted");
		});

	}
}
