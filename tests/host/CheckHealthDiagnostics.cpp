#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/host/Diagnostics.h>
#include <DearModdingUI/pages/Health.h>
#include <DearModdingUI/pages/Home.h>
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
		runner.test("Health creates a section for each bridge source", [] {
			const std::vector<RegisteredClient> clients{
				{ .handle = 1, .id = "native-z", .displayName = "Zulu Native" },
				{ .handle = 6, .id = "native-a", .displayName = "Alpha Native" },
				{
					.handle = 2,
					.id = "zeta-two",
					.displayName = "Beta",
					.origin = DMUI_CLIENT_ORIGIN_BRIDGED,
					.bridgeSourceLabel = "Zeta"
				},
				{
					.handle = 3,
					.id = "alpha",
					.displayName = "Alpha",
					.origin = DMUI_CLIENT_ORIGIN_BRIDGED,
					.bridgeSourceLabel = "Alpha"
				},
				{
					.handle = 4,
					.id = "zeta-one",
					.displayName = "Able",
					.origin = DMUI_CLIENT_ORIGIN_BRIDGED,
					.bridgeSourceLabel = "Zeta"
				}
			};

			const auto sections = BuildHealthClientSections(clients);
			require(
				sections.size() == 3 &&
					sections[0].heading == "Registered mods" &&
					sections[0].clients.size() == 2 &&
					sections[0].clients[0]->id == "native-a" &&
					sections[0].clients[1]->id == "native-z" &&
					sections[1].heading == "Alpha mods" &&
					sections[1].clients.size() == 1 &&
					sections[1].clients[0]->id == "alpha" &&
					sections[2].heading == "Zeta mods" &&
					sections[2].clients.size() == 2 &&
					sections[2].clients[0]->id == "zeta-one" &&
					sections[2].clients[1]->id == "zeta-two",
				"bridge source sections or their member order were unstable");

			const std::vector<RegisteredClient> unnamedBridge{
				{
					.handle = 5,
					.id = "unnamed",
					.displayName = "Unnamed",
					.origin = DMUI_CLIENT_ORIGIN_BRIDGED
				}
			};
			const auto fallback = BuildHealthClientSections(unnamedBridge);
			require(
				fallback.size() == 1 &&
					fallback.front().heading == "Bridged mods",
				"an unnamed bridge did not receive the generic heading");
		});

		runner.test("overdue Health observations warn consistently without changing capability", [] {
			const auto start = HealthClock::time_point{} +
				std::chrono::seconds{ 10 };
			const auto deadline = start + std::chrono::seconds{ 10 };
			std::array snapshots{
				HealthSnapshot{
					"dmui.render.reconciliation",
					HealthState::kWaiting,
					start,
					deadline,
					"Waiting for the renderer." }
			};
			const auto before = deadline - std::chrono::seconds{ 1 };
			require(BuildHomeHealthSummary(snapshots, 0, before) ==
						"1 host subsystem starting" &&
					HomeHealthSeverity(snapshots, 0, before) ==
						HealthSeverity::kNeutral &&
					BuildHealthSubsystemRows(snapshots, before)[0].stateLabel ==
						"Waiting",
				"an expected wait was escalated before its deadline");

			const auto rows = BuildHealthSubsystemRows(snapshots, deadline);
			require(BuildHomeHealthSummary(snapshots, 0, deadline) ==
						"1 host subsystem needs attention" &&
					HomeHealthSeverity(snapshots, 0, deadline) ==
						HealthSeverity::kWarning &&
					rows[0].state == HealthState::kWaiting &&
					rows[0].stateLabel == "Waiting (deadline exceeded)" &&
					rows[0].reason == "Waiting for the renderer." &&
					HealthStatusSeverity(rows[0].severity) ==
						DMUI_STATUS_SEVERITY_WARNING,
				"an overdue wait disappeared from Home or Health");
			const auto report = BuildHealthDiagnosticsReport(
				"Host", "0.1.0", snapshots, {}, {}, {}, deadline);
			require(report.find("Waiting (deadline exceeded)") !=
						std::string::npos &&
					snapshots[0].state == HealthState::kWaiting,
				"the copied report omitted expiry or presentation changed capability");

			snapshots[0].state = HealthState::kProgressing;
			require(HomeHealthSeverity(snapshots, 0, deadline) ==
						HealthSeverity::kWarning &&
					BuildHealthSubsystemRows(snapshots, deadline)[0].stateLabel ==
						"Progressing (deadline exceeded)",
				"overdue progress was presented as normal startup");
			snapshots[0].state = HealthState::kFailed;
			require(HomeHealthSeverity(snapshots, 0, deadline) ==
						HealthSeverity::kError &&
					BuildHealthSubsystemRows(snapshots, deadline)[0].severity ==
						HealthSeverity::kError,
				"deadline warning downgraded a failed subsystem");
			snapshots[0].state = HealthState::kReady;
			require(BuildHomeHealthSummary(snapshots, 0, deadline) ==
						"All systems ready" &&
					HomeHealthSeverity(snapshots, 0, deadline) ==
						HealthSeverity::kSuccess &&
					BuildHealthSubsystemRows(snapshots, deadline)[0].stateLabel ==
						"Ready",
				"recovered readiness retained a stale deadline warning");
		});

		runner.test("client diagnostics aggregate by severity scope and summary", [] {
			DiagnosticStore store;
			DMUI_DiagnosticDescriptor diagnostic{
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
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
		});

		runner.test("client diagnostic retention stays bounded", [] {
			DiagnosticStore store;
			std::vector<std::string> summaries;
			summaries.reserve(kDiagnosticRecordLimitPerClient + 2);
			for (size_t index = 0;
				index < kDiagnosticRecordLimitPerClient + 2;
				++index)
			{
				summaries.push_back(
					"Diagnostic " + std::to_string(index));
				const DMUI_DiagnosticDescriptor diagnostic{
					DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
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
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
				DMUI_STATUS_SEVERITY_WARNING,
				"General",
				summaries.front().c_str(),
				nullptr
			};
			require(
				store.Report(9, repeated) == DMUI_RESULT_OK,
				"an existing diagnostic stopped incrementing at the cap");
			const DMUI_DiagnosticDescriptor repeatedDropped{
				DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE,
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

		runner.test("Health diagnostics report includes support context", [] {
			const std::vector<RegisteredClient> clients{
				{
					.handle = 4,
					.id = "example.mod",
					.displayName = "Example Mod",
					.version = DMUI_MAKE_VERSION(2, 5)
				}
			};
			const std::array statuses{
				ClientStatus{
					4,
					DMUI_STATUS_SEVERITY_WARNING
				}
			};
			const std::array subsystems{
				HealthSnapshot{
					"dmui.render.reconciliation",
					HealthState::kReady,
					{},
					{},
					"renderer attached"
				}
			};
			const std::array diagnostics{
				ClientDiagnosticSnapshot{
					4,
					{
						ClientDiagnosticRecord{
							4,
							DMUI_STATUS_SEVERITY_ERROR,
							"General",
							"Value could not be loaded.",
							"Missing setting id.",
							3
						}
					},
					2
				}
			};

			const auto report = BuildHealthDiagnosticsReport(
				"Evil Modding",
				"1.0.0",
				subsystems,
				clients,
				statuses,
				diagnostics);
			require(
				report.find("Evil Modding 1.0.0") != std::string::npos &&
					report.find("dmui.render.reconciliation: Ready") !=
						std::string::npos &&
					report.find("Example Mod 2.5 [Warning]") !=
						std::string::npos &&
					report.find("Value could not be loaded. (x3)") !=
						std::string::npos &&
					report.find(
						"2 further diagnostic reports were not retained.") !=
						std::string::npos,
				"the copied Health report omitted required support context");
		});

		runner.test("Home summarizes readiness and attention states", [] {
			require(
				BuildHomeHealthSummary({}, 0) ==
					"Host health not observed yet" &&
					HomeHealthSeverity({}, 0) == HealthSeverity::kNeutral,
				"an empty health registry claimed readiness");

			const std::array ready{
				HealthSnapshot{
					"dmui.render.reconciliation",
					HealthState::kReady,
					{},
					{},
					{} }
			};
			require(
				BuildHomeHealthSummary(ready, 0) == "All systems ready" &&
					HomeHealthSeverity(ready, 0) ==
						HealthSeverity::kSuccess,
				"ready health did not produce the success summary");

			const std::array waiting{
				HealthSnapshot{
					"dmui.render.reconciliation",
					HealthState::kWaiting,
					{},
					{},
					"renderer data is not initialized" }
			};
			require(
				BuildHomeHealthSummary(waiting, 2) ==
					"1 host subsystem starting; 2 mods need attention",
				"startup state did not remain distinct from client attention");

			const std::array degraded{
				HealthSnapshot{
					"dmui.configuration",
					HealthState::kDegraded,
					{},
					{},
					"Using defaults after a parse failure." }
			};
			require(
				BuildHomeHealthSummary(degraded, 0) ==
					"1 host subsystem needs attention" &&
					HomeHealthSeverity(degraded, 0) ==
						HealthSeverity::kWarning,
				"a degraded subsystem did not need attention");

			const std::array failed{
				HealthSnapshot{
					"dmui.input.game-interception",
					HealthState::kFailed,
					{},
					{},
					"PlayerCamera patch failed." }
			};
			require(
				HomeHealthSeverity(failed, 0) == HealthSeverity::kError,
				"a failed subsystem did not color the Home summary as an error");
		});

	}
}
