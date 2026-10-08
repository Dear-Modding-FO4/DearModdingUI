#include <DearModdingUI/pages/Home.h>

#include <DearModdingUI/localization/Localization.h>

#include <array>
#include <utility>

namespace DearModdingUI
{
	namespace
	{
		constexpr std::array<HomeQuickLink, 2> kQuickLinks{
			HomeQuickLink{
				"GitHub",
				"https://github.com/Dear-Modding-FO4/DearModdingUI",
				{},
				"github-logo",
				true },
			HomeQuickLink{
				"Nexus Mods",
				"https://www.nexusmods.com/fallout4/mods/108874",
				{},
				{},
				true }
		};
	}

	std::string_view HomeAboutText() noexcept
	{
		return Localization::Text(
			"$DMUI_Home_About",
			"DearModdingUI is a shared settings menu for Fallout 4. "
			"Mods register their own pages in one overlay instead of each "
			"shipping a separate menu, and mods that were never built for it "
			"can appear here too.");
	}

	std::span<const HomeQuickLink> HomeQuickLinks() noexcept
	{
		return kQuickLinks;
	}

	std::vector<HomeFaqEntry> BuildHomeFaq(
		std::string_view a_toggleKeyName)
	{
		return {
			{
				Localization::Text(
					"$DMUI_Home_FaqOpenQuestion",
					"How do I open the menu?"),
				Localization::Format(
					"$DMUI_Home_FaqOpenAnswer",
					"Press {} to open or close DearModdingUI. You can change "
					"this key on the Settings page.",
					a_toggleKeyName)
			},
			{
				Localization::Text(
					"$DMUI_Home_FaqStorageQuestion",
					"Where are settings stored?"),
				Localization::Text(
					"$DMUI_Home_FaqStorageAnswer",
					"Host settings are stored in "
					"Data/F4SE/Plugins/DearModdingUI.toml.")
			},
			{
				Localization::Text(
					"$DMUI_Home_FaqMissingPageQuestion",
					"Why is a mod page missing or grayed out?"),
				Localization::Text(
					"$DMUI_Home_FaqMissingPageAnswer",
					"Open the Health page to see whether the host or that mod "
					"reported a problem.")
			},
			{
				Localization::Text(
					"$DMUI_Home_FaqReplaceQuestion",
					"Does this replace a mod's own menu?"),
				Localization::Text(
					"$DMUI_Home_FaqReplaceAnswer",
					"No. Mods register pages and DearModdingUI draws them in the "
					"shared overlay.")
			}
		};
	}

	std::string BuildHomeHealthSummary(
		std::span<const HealthSnapshot> a_subsystems,
		size_t a_clientsNeedingAttention,
		HealthClock::time_point a_now)
	{
		if (a_subsystems.empty() && a_clientsNeedingAttention == 0)
			return Localization::Text(
				"$DMUI_Home_HealthNotObserved",
				"Host health not observed yet");

		size_t subsystemsNeedingAttention{};
		size_t subsystemsStarting{};
		for (const auto& subsystem : a_subsystems)
		{
			if (HealthNeedsAttention(subsystem, a_now))
				++subsystemsNeedingAttention;
			else if (HealthStateIsStarting(subsystem.state))
				++subsystemsStarting;
		}
		if (subsystemsNeedingAttention == 0 &&
			subsystemsStarting == 0 &&
			a_clientsNeedingAttention == 0)
			return Localization::Text(
				"$DMUI_Home_AllReady",
				"All systems ready");

		if (subsystemsNeedingAttention != 0 &&
			a_clientsNeedingAttention != 0)
		{
			return Localization::Format(
				"$DMUI_Home_SubsystemsAndModsAttention",
				"Host subsystems needing attention: {}; mods needing attention: {}",
				subsystemsNeedingAttention,
				a_clientsNeedingAttention);
		}
		if (subsystemsStarting != 0 &&
			a_clientsNeedingAttention != 0)
		{
			return Localization::Format(
				"$DMUI_Home_SubsystemsStartingModsAttention",
				"Host subsystems starting: {}; mods needing attention: {}",
				subsystemsStarting,
				a_clientsNeedingAttention);
		}
		if (subsystemsNeedingAttention != 0)
		{
			return Localization::Format(
				"$DMUI_Home_SubsystemsAttention",
				"Host subsystems needing attention: {}",
				subsystemsNeedingAttention);
		}
		if (subsystemsStarting != 0)
		{
			return Localization::Format(
				"$DMUI_Home_SubsystemsStarting",
				"Host subsystems starting: {}",
				subsystemsStarting);
		}
		return Localization::Format(
			"$DMUI_Home_ModsAttention",
			"Mods needing attention: {}",
			a_clientsNeedingAttention);
	}

	HealthSeverity HomeHealthSeverity(
		std::span<const HealthSnapshot> a_subsystems,
		size_t a_clientsNeedingAttention,
		HealthClock::time_point a_now) noexcept
	{
		auto severity = a_subsystems.empty() ?
			HealthSeverity::kNeutral :
			HealthSeverity::kSuccess;
		for (const auto& subsystem : a_subsystems)
		{
			switch (HealthSnapshotSeverity(subsystem, a_now))
			{
			case HealthSeverity::kError:
				return HealthSeverity::kError;
			case HealthSeverity::kWarning:
				severity = HealthSeverity::kWarning;
				break;
			case HealthSeverity::kInfo:
				if (severity != HealthSeverity::kWarning)
					severity = HealthSeverity::kInfo;
				break;
			case HealthSeverity::kNeutral:
				if (severity == HealthSeverity::kSuccess)
					severity = HealthSeverity::kNeutral;
				break;
			default:
				break;
			}
		}
		if (a_clientsNeedingAttention != 0 &&
			severity != HealthSeverity::kError)
			severity = HealthSeverity::kWarning;
		return severity;
	}
}
