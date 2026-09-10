#pragma once

#include <DearModdingUI/ThemeDefaults.h>
#include <Support/SubsystemHealth.h>

#include <array>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace DearModdingUI::Theme
{
	struct TypographyLoadOutcome
	{
		std::string requestedFamily;
		std::string effectiveFamily;
		bool requestedFamilyFound{ false };
		bool requestedBodyLoaded{ false };
		std::array<bool, static_cast<size_t>(FontRole::kCount)> rolesLoaded{};
		bool iconsLoaded{ false };
		bool emergencyFontUsed{ false };
		bool usableAtlas{ false };
	};

	[[nodiscard]] inline HealthObservation ClassifyTypographyHealth(
		const TypographyLoadOutcome& a_outcome)
	{
		if (!a_outcome.usableAtlas)
		{
			return {
				HealthState::kFailed,
				"No usable font atlas could be prepared. Verify the bundled font files and restart."
			};
		}

		std::string reason;
		const auto append = [&](std::string_view a_detail) {
			if (!reason.empty())
				reason.push_back(' ');
			reason.append(a_detail);
		};
		if (!a_outcome.requestedFamilyFound)
		{
			append(std::format(
				"Requested family \"{}\" was unavailable; using \"{}\".",
				a_outcome.requestedFamily,
				a_outcome.effectiveFamily));
		}
		else if (!a_outcome.requestedBodyLoaded)
		{
			append(std::format(
				"Body font for \"{}\" could not load; using \"{}\".",
				a_outcome.requestedFamily,
				a_outcome.effectiveFamily));
		}

		constexpr std::array roleNames{
			"body",
			"title",
			"heading",
			"subheading",
			"subtext"
		};
		std::string missingRoles;
		for (size_t index = 0; index < a_outcome.rolesLoaded.size(); ++index)
		{
			if (a_outcome.rolesLoaded[index])
				continue;
			if (!missingRoles.empty())
				missingRoles.append(", ");
			missingRoles.append(roleNames[index]);
		}
		if (!missingRoles.empty())
		{
			append(std::format(
				"Bundled {} role{} used fallback text.",
				missingRoles,
				missingRoles.contains(',') ? "s" : ""));
		}
		if (!a_outcome.iconsLoaded)
			append("Phosphor icons are unavailable; labels remain text-only.");
		if (a_outcome.emergencyFontUsed)
			append("The built-in emergency font is keeping text usable.");

		if (!reason.empty())
			return { HealthState::kDegraded, std::move(reason) };
		return {
			HealthState::kReady,
			std::format(
				"Loaded \"{}\" with all bundled roles and Phosphor icons.",
				a_outcome.effectiveFamily)
		};
	}
}
