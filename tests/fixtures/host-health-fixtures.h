#pragma once

#include <Support/SubsystemHealth.h>

#include <memory>
#include <vector>

namespace DmuiTestFixtures
{
	[[nodiscard]] inline std::vector<std::unique_ptr<DearModdingUI::SubsystemHealth>>
		CreateSyntheticHealth(
			DearModdingUI::SubsystemHealthRegistry& a_registry,
			DearModdingUI::HealthReporter& a_reporter)
	{
		using namespace DearModdingUI;
		std::vector<std::unique_ptr<SubsystemHealth>> result;
		const auto add = [&](std::string_view identity,
							 HealthState state,
							 std::string_view reason) {
			auto health = std::make_unique<SubsystemHealth>(
				identity, a_reporter, a_registry);
			health->Observe(state, reason);
			result.push_back(std::move(health));
		};
		add(
			"preview.synthetic.configuration",
			HealthState::kReady,
			"Synthetic fixture: using defaults; configuration file is absent.");
		add(
			"preview.synthetic.typography",
			HealthState::kDegraded,
			"Synthetic fixture: requested family is unavailable; using Jost with text-only labels.");
		add(
			"preview.synthetic.input",
			HealthState::kFailed,
			"Synthetic fixture: PlayerCamera receiver patch failed; check for an incompatible input hook.");
		return result;
	}
}
