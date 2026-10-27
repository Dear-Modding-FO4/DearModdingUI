#pragma once

#include <DearModdingUI/ThemeDefaults.h>
#include <Support/Message.h>
#include <Support/SubsystemHealth.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

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
		size_t systemFallbacksLoaded{ 0 };
		bool emergencyFontUsed{ false };
		bool usableAtlas{ false };
	};

	[[nodiscard]] inline HealthObservation ClassifyTypographyHealth(
		const TypographyLoadOutcome& a_outcome)
	{
		using Support::Message;
		using Support::Phrase;
		using Support::PluralPhrase;

		if (!a_outcome.usableAtlas)
		{
			return {
				HealthState::kFailed,
				Message{ Phrase{
					"$DMUI_Typography_NoAtlas",
					"No usable font atlas could be prepared. Verify the bundled font files and restart." } }
			};
		}

		std::vector<Message> sentences;
		if (!a_outcome.requestedFamilyFound)
		{
			sentences.emplace_back(
				Phrase{
					"$DMUI_Typography_FamilyUnavailable",
					"Requested family \"{}\" was unavailable; using \"{}\"." },
				a_outcome.requestedFamily,
				a_outcome.effectiveFamily);
		}
		else if (!a_outcome.requestedBodyLoaded)
		{
			sentences.emplace_back(
				Phrase{
					"$DMUI_Typography_BodyFontFailed",
					"Body font for \"{}\" could not load; using \"{}\"." },
				a_outcome.requestedFamily,
				a_outcome.effectiveFamily);
		}

		std::vector<Message> missingRoles;
		for (size_t index = 0; index < a_outcome.rolesLoaded.size(); ++index)
		{
			if (!a_outcome.rolesLoaded[index])
				missingRoles.push_back(Message::Text(std::string{ kFontRoleDefaults[index].key }));
		}
		if (!missingRoles.empty())
		{
			const auto count = missingRoles.size();
			sentences.push_back(Message::Plural(
				PluralPhrase{
					"$DMUI_Typography_MissingRoles",
					"Bundled {} role used fallback text.",
					"Bundled {} roles used fallback text." },
				count,
				Message::Join(std::move(missingRoles), ", ")));
		}
		if (!a_outcome.iconsLoaded)
		{
			sentences.emplace_back(Phrase{
				"$DMUI_Typography_IconsUnavailable",
				"Phosphor icons are unavailable; labels remain text-only." });
		}
		if (a_outcome.systemFallbacksLoaded == 0)
		{
			sentences.emplace_back(Phrase{
				"$DMUI_Typography_NoSystemFallbacks",
				"No system fallback fonts loaded; non-Latin text may not render." });
		}
		if (a_outcome.emergencyFontUsed)
		{
			sentences.emplace_back(Phrase{
				"$DMUI_Typography_EmergencyFont",
				"The built-in emergency font is keeping text usable." });
		}

		if (!sentences.empty())
			return { HealthState::kDegraded, Message::Join(std::move(sentences), " ") };
		return {
			HealthState::kReady,
			Message::Plural(
				PluralPhrase{
					"$DMUI_Typography_Ready",
					"Loaded \"{}\" with all bundled roles, Phosphor icons, and {} system fallback font.",
					"Loaded \"{}\" with all bundled roles, Phosphor icons, and {} system fallback fonts." },
				a_outcome.systemFallbacksLoaded,
				a_outcome.effectiveFamily,
				a_outcome.systemFallbacksLoaded)
		};
	}
}
