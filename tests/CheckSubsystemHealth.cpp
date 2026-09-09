#include <Support/SubsystemHealth.h>
#include <host-health-fixtures.h>

#include "Harness.h"

#include <chrono>
#include <memory>
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

		runner.test("synthetic health fixtures use the caller's registry and lifetime", [] {
			CapturingHealthReporter reporter;
			SubsystemHealthRegistry registry;
			auto fixtures = DmuiTestFixtures::CreateSyntheticHealth(registry, reporter);
			const auto snapshots = registry.Snapshots();
			require(fixtures.size() == 3 && snapshots.size() == 3 &&
					reporter.records.size() == 3,
				"synthetic health fixtures were not registered");
			for (const auto& snapshot : snapshots)
				require(snapshot.identity.starts_with("preview.synthetic.") &&
						snapshot.reason.starts_with("Synthetic fixture:"),
					"synthetic health was not clearly labeled");
			fixtures.clear();
			require(registry.Snapshots().empty(),
				"synthetic health outlived its owning application");
		});

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

		runner.test("health deadline progress and recovery stay distinct", [] {
			CapturingHealthReporter reporter;
			SubsystemHealth health{ "fixture", reporter, start };
			health.Observe(HealthState::kWaiting, "dependency is unavailable", start);
			health.SetDeadline(start + 10s);
			health.Evaluate(start + 10s);
			health.Observe(HealthState::kProgressing, "dependency connected", start + 11s);
			health.Observe(HealthState::kReady, "dependency ready", start + 12s);
			require(
				reporter.records.size() == 4 &&
					reporter.records[2].event ==
						HealthEvent::kDeadlineProgress &&
					reporter.records[3].event ==
						HealthEvent::kDeadlineRecovery &&
					health.Snapshot().state == HealthState::kReady,
				"deadline progress was reported as full recovery");
		});

		runner.test("degraded state recovers after an intervening rebuild", [] {
			CapturingHealthReporter reporter;
			SubsystemHealth health{ "fixture", reporter, start };
			health.Observe(HealthState::kDegraded, "using fallback", start);
			health.Observe(
				HealthState::kProgressing,
				"rebuilding",
				start + 1s);
			health.Observe(HealthState::kReady, "rebuild complete", start + 2s);
			require(
				reporter.records.size() == 3 &&
					reporter.records[1].event == HealthEvent::kTransition &&
					reporter.records.back().event == HealthEvent::kRecovery,
				"full recovery did not require a ready observation");
		});

		runner.test("failed state reports a genuine ready recovery", [] {
			CapturingHealthReporter reporter;
			SubsystemHealth health{ "fixture", reporter, start };
			health.Observe(HealthState::kFailed, "capability absent", start);
			health.Observe(HealthState::kReady, "capability restored", start + 1s);
			require(
				reporter.records.size() == 2 &&
					reporter.records.back().event == HealthEvent::kRecovery,
				"a failed capability did not report full recovery");
		});

		runner.test("health state helpers share readiness and severity", [] {
			require(
				HealthStateLabel(HealthState::kDegraded) == "Degraded" &&
					HealthStateLabel(HealthState::kFailed) == "Failed" &&
					HealthStateSeverity(HealthState::kWaiting) ==
						HealthSeverity::kNeutral &&
					HealthStateSeverity(HealthState::kDegraded) ==
						HealthSeverity::kWarning &&
					HealthStateSeverity(HealthState::kFailed) ==
						HealthSeverity::kError &&
					HealthStateIsStarting(HealthState::kWaiting) &&
					!HealthStateNeedsAttention(HealthState::kProgressing) &&
					HealthStateNeedsAttention(HealthState::kDegraded) &&
					HealthStateIsUsable(HealthState::kDegraded) &&
					!HealthStateIsReady(HealthState::kDegraded),
				"health state helpers disagreed about startup or fallback states");
		});

		runner.test("health snapshots own dynamic identity and reason text", [] {
			CapturingHealthReporter reporter;
			std::string identity{ "dynamic.identity" };
			SubsystemHealth health{ identity, reporter, start };
			std::string reason{ "temporary reason" };
			health.Observe(HealthState::kDegraded, reason, start);
			auto snapshot = health.Snapshot();
			identity.assign("changed");
			reason.assign("destroyed");
			health.Observe(HealthState::kReady, "healthy", start + 1s);
			require(
				snapshot.identity == "dynamic.identity" &&
					snapshot.reason == "temporary reason" &&
					reporter.records.front().snapshot.reason ==
						"temporary reason",
				"health snapshots retained borrowed diagnostic text");
		});

		runner.test("copied health snapshots survive later observations", [] {
			CapturingHealthReporter reporter;
			SubsystemHealthRegistry registry;
			SubsystemHealth health{ "fixture", reporter, registry, start };
			health.Observe(HealthState::kDegraded, "first reason", start);
			const auto first = registry.Snapshots();
			health.Observe(HealthState::kFailed, "second reason", start + 1s);
			require(
				first.size() == 1 &&
					first.front().state == HealthState::kDegraded &&
					first.front().reason == "first reason",
				"a copied registry snapshot changed after re-observation");
		});

		runner.test("scoped health fixtures unregister before their registry", [start] {
			CapturingHealthReporter reporter;
			SubsystemHealthRegistry registry;
			{
				std::vector<std::unique_ptr<SubsystemHealth>> fixtures;
				auto fixture = std::make_unique<SubsystemHealth>(
					"preview.synthetic.input", reporter, registry, start);
				require(fixture->Observe(
							HealthState::kFailed,
							"Synthetic input failure.",
							start),
					"synthetic observation was not retained");
				fixtures.push_back(std::move(fixture));
				require(registry.Snapshots().size() == 1,
					"scoped fixture was not registered");
			}
			require(registry.Snapshots().empty(),
				"destroyed fixture left a dangling registry entry");
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
