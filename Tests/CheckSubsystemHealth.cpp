#include <Support/SubsystemHealth.h>

#include "Harness.h"

#include <chrono>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;

	namespace
	{
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

		runner.test("health transition logs once and identical observations stay silent", [] {
			CapturingHealthReporter reporter;
			SubsystemHealth health{ "fixture", reporter, start };
			health.Observe(HealthState::kWaiting, "dependency is unavailable", start);
			health.Observe(HealthState::kWaiting, "dependency is unavailable", start + 1s);
			require(
				reporter.records.size() == 1 &&
					reporter.records.front().event == HealthEvent::kTransition &&
					reporter.records.front().snapshot.enteredAt == start,
				"an identical health observation logged more than once");
		});

		runner.test("health deadline escalates without changing capability", [] {
			CapturingHealthReporter reporter;
			SubsystemHealth health{ "fixture", reporter, start };
			health.Observe(HealthState::kWaiting, "dependency is unavailable", start);
			health.SetDeadline(start + 10s);
			health.Evaluate(start + 10s);
			health.Evaluate(start + 20s);
			require(
				reporter.records.size() == 2 &&
					reporter.records.back().event ==
						HealthEvent::kDeadlineExceeded &&
					health.Snapshot().state == HealthState::kWaiting &&
					health.Snapshot().reason == "dependency is unavailable",
				"a diagnostic deadline changed capability or repeated");
		});

		runner.test("health recovery after a deadline is reported", [] {
			CapturingHealthReporter reporter;
			SubsystemHealth health{ "fixture", reporter, start };
			health.Observe(HealthState::kWaiting, "dependency is unavailable", start);
			health.SetDeadline(start + 10s);
			health.Evaluate(start + 10s);
			health.Observe(HealthState::kProgressing, "dependency connected", start + 11s);
			require(
				reporter.records.size() == 3 &&
					reporter.records.back().event == HealthEvent::kRecovery &&
					health.Snapshot().state == HealthState::kProgressing,
				"recovery after deadline was not reported");
		});

		runner.test("health registry returns each subsystem's live observation", [] {
			CapturingHealthReporter reporter;
			SubsystemHealthRegistry registry;
			SubsystemHealth health{ "fixture", reporter, registry, start };
			require(registry.Snapshots().empty(),
				"an unobserved subsystem exposed a guessed state");

			health.Observe(
				HealthState::kWaiting,
				"dependency is unavailable",
				start);
			auto snapshots = registry.Snapshots();
			require(
				snapshots.size() == 1 &&
					snapshots.front().state == HealthState::kWaiting &&
					snapshots.front().reason == "dependency is unavailable",
				"the registry did not return the waiting observation");

			health.Observe(HealthState::kReady, {}, start + 5s);
			snapshots = registry.Snapshots();
			require(
				snapshots.size() == 1 &&
					snapshots.front().state == HealthState::kReady &&
					snapshots.front().enteredAt == start + 5s,
				"the registry returned a stale subsystem observation");
		});
	}
}
