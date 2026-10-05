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

	void run_health_diagnostics_checks(Runner& runner)
	{
		runner.test("client diagnostic retention stays bounded", [] {
			DiagnosticStore store;
			{
				DMUI_DiagnosticDescriptor diagnostic{
					DMUI_STATUS_SEVERITY_WARNING,
					"General",
					"Expected a boolean value.",
					"First location"
				};
				require(
					store.Report(7, diagnostic) == DMUI_RESULT_OK &&
						store.Report(7, diagnostic) == DMUI_RESULT_OK,
					"matching diagnostics were rejected");
				diagnostic.detail = "Later location";
				diagnostic.scope = "Advanced";
				require(
					store.Report(7, diagnostic) == DMUI_RESULT_OK,
					"a distinct diagnostic scope was rejected");
				diagnostic.scope = "General";
				diagnostic.severity = DMUI_STATUS_SEVERITY_ERROR;
				require(
					store.Report(7, diagnostic) == DMUI_RESULT_OK,
					"a distinct diagnostic severity was rejected");

				const auto snapshot = store.Snapshot(7);
				require(
					snapshot &&
						snapshot->records.size() == 3 &&
						snapshot->records[0].occurrenceCount == 2 &&
						snapshot->records[0].detail == "First location",
					"diagnostic aggregation or first-detail retention changed");
			}

			std::vector<std::string> summaries;
			summaries.reserve(kDiagnosticRecordLimitPerClient + 2);
			for (size_t index = 0;
				index < kDiagnosticRecordLimitPerClient + 2;
				++index)
			{
				summaries.push_back(
					"Diagnostic " + std::to_string(index));
				const DMUI_DiagnosticDescriptor diagnostic{
					DMUI_STATUS_SEVERITY_WARNING,
					"General",
					summaries.back().c_str(),
					nullptr
				};
				require(
					store.Report(9, diagnostic) == DMUI_RESULT_OK,
					"a bounded diagnostic report was rejected");
			}
			const DMUI_DiagnosticDescriptor repeated{
				DMUI_STATUS_SEVERITY_WARNING,
				"General",
				summaries.front().c_str(),
				nullptr
			};
			require(
				store.Report(9, repeated) == DMUI_RESULT_OK,
				"an existing diagnostic stopped incrementing at the cap");
			const DMUI_DiagnosticDescriptor repeatedDropped{
				DMUI_STATUS_SEVERITY_WARNING,
				"General",
				summaries[kDiagnosticRecordLimitPerClient].c_str(),
				nullptr
			};
			require(
				store.Report(9, repeatedDropped) == DMUI_RESULT_OK &&
					store.Report(9, repeatedDropped) == DMUI_RESULT_OK,
				"a dropped diagnostic report was rejected");

			const auto snapshot = store.Snapshot(9);
			require(
				snapshot &&
					snapshot->records.size() ==
						kDiagnosticRecordLimitPerClient &&
					snapshot->droppedReportCount == 4 &&
					snapshot->records.front().occurrenceCount == 2,
				"bounded diagnostic retention lost counts or admitted overflow");
		});
	}
}
