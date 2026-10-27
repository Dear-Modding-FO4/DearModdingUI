#include <Support/SubsystemHealth.h>

#include "../Harness.h"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;

	namespace
	{
		[[nodiscard]] Support::Message Reason(std::string_view a_text)
		{
			return Support::Message::Text(std::string{ a_text });
		}

		struct HealthRecord
		{
			HealthEvent event;
			HealthSnapshot snapshot;
		};

		class CapturingHealthReporter final : public HealthReporter
		{
		public:
			void Report(
				HealthEvent a_event,
				const HealthSnapshot& a_snapshot) noexcept override
			{
				records.push_back({ a_event, a_snapshot });
			}

			std::vector<HealthRecord> records;
		};
	}

	void run_subsystem_health_checks(Runner& runner)
	{
		using namespace std::chrono_literals;
		constexpr auto start = HealthClock::time_point{ 10s };

		runner.test("health deadlines deduplicate escalation and distinguish recovery", [start] {
			CapturingHealthReporter reporter;
			SubsystemHealth health{ "fixture", reporter, start };
			health.Observe(HealthState::kWaiting, Reason("dependency is unavailable"), start);
			health.Observe(HealthState::kWaiting, Reason("dependency is unavailable"), start + 1s);
			require(
				reporter.records.size() == 1 &&
					reporter.records.front().event == HealthEvent::kTransition &&
					reporter.records.front().snapshot.enteredAt == start,
				"an identical health observation logged more than once");
			health.SetDeadline(start + 10s);
			health.Evaluate(start + 9s);
			require(reporter.records.size() == 1,
				"the health deadline escalated early");
			health.Evaluate(start + 10s);
			health.Evaluate(start + 20s);
			require(
				reporter.records.size() == 2 &&
					reporter.records.back().event ==
						HealthEvent::kDeadlineExceeded &&
					health.Snapshot().state == HealthState::kWaiting &&
					health.Snapshot().reason.English() == "dependency is unavailable",
				"a diagnostic deadline changed capability or repeated");
			health.Observe(HealthState::kProgressing, Reason("dependency connected"), start + 21s);
			health.Observe(HealthState::kReady, Reason("dependency ready"), start + 22s);
			health.Evaluate(start + 30s);
			require(
				reporter.records.size() == 4 &&
					reporter.records[2].event ==
						HealthEvent::kDeadlineProgress &&
					reporter.records[3].event ==
						HealthEvent::kDeadlineRecovery &&
					health.Snapshot().state == HealthState::kReady,
				"deadline progress was reported as full recovery");

			CapturingHealthReporter recoveryReporter;
			SubsystemHealth recovering{ "fixture", recoveryReporter, start };
			recovering.Observe(HealthState::kDegraded, Reason("using fallback"), start);
			recovering.Observe(HealthState::kProgressing, Reason("rebuilding"), start + 1s);
			require(recoveryReporter.records.back().event == HealthEvent::kTransition,
				"an intervening rebuild claimed full recovery");
			recovering.Observe(HealthState::kReady, Reason("rebuild complete"), start + 2s);
			require(recoveryReporter.records.size() == 3 &&
					recoveryReporter.records.back().event == HealthEvent::kRecovery,
				"a rebuilt subsystem did not report full recovery");

			recovering.Observe(HealthState::kFailed, Reason("capability absent"), start + 3s);
			recovering.Observe(HealthState::kReady, Reason("capability restored"), start + 4s);
			require(recoveryReporter.records.size() == 5 &&
					recoveryReporter.records.back().event == HealthEvent::kRecovery,
				"a failed subsystem did not report full recovery");
		});

		runner.test("health observations compare whole structured reasons", [start] {
			constexpr Support::Phrase phrase{ "$DMUI_Check_Reason", "Using {}." };
			CapturingHealthReporter reporter;
			SubsystemHealth health{ "fixture", reporter, start };
			health.Observe(HealthState::kDegraded, Support::Message{ phrase, "Jost" }, start);
			health.Observe(HealthState::kDegraded, Support::Message{ phrase, "Jost" }, start + 1s);
			require(reporter.records.size() == 1,
				"an identical structured reason was published twice");
			health.Observe(HealthState::kDegraded, Support::Message{ phrase, "Arial" }, start + 2s);
			require(reporter.records.size() == 2 &&
					reporter.records.back().snapshot.reason.English() == "Using Arial.",
				"a changed argument was deduplicated as the same reason");
		});

		runner.test("health registry owns snapshots and removes destroyed subsystems", [start] {
			CapturingHealthReporter reporter;
			SubsystemHealthRegistry registry;
			HealthSnapshot snapshot;
			std::vector<HealthSnapshot> snapshots;
			{
				std::string identity{ "dynamic.identity" };
				std::string reason{ "temporary reason" };
				SubsystemHealth health{ identity, reporter, registry, start };
				require(registry.Snapshots().empty(),
					"an unobserved subsystem exposed a guessed state");
				health.Observe(HealthState::kDegraded, Reason(reason), start);
				snapshot = health.Snapshot();
				snapshots = registry.Snapshots();
				identity.assign("changed");
				reason.assign("destroyed");
				health.Observe(HealthState::kReady, {}, start + 5s);

				const auto live = registry.Snapshots();
				require(live.size() == 1 &&
						live.front().identity == "dynamic.identity" &&
						live.front().state == HealthState::kReady &&
						live.front().reason.empty() &&
						live.front().enteredAt == start + 5s,
					"the registry returned a stale subsystem observation");
			}
			require(registry.Snapshots().empty(),
				"a destroyed subsystem left a dangling registry entry");
			require(snapshot.identity == "dynamic.identity" &&
					snapshot.reason.English() == "temporary reason" &&
					snapshots.size() == 1 &&
					snapshots.front().state == HealthState::kDegraded &&
					snapshots.front().reason.English() == "temporary reason" &&
					reporter.records.front().snapshot.reason.English() == "temporary reason",
				"retained snapshots changed with their source or subsystem");
		});
	}
}
