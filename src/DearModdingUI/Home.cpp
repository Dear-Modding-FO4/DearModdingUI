#include <DearModdingUI/Home.h>

#include <array>
#include <cstdio>
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
				{},
				"DearModdingUI is not published on Nexus Mods yet.",
				{},
				false }
		};
	}

	std::string_view HomeAboutText() noexcept
	{
		return "DearModdingUI is a shared settings menu for Fallout 4. "
			"Mods register their own pages in one overlay instead of each "
			"shipping a separate menu, and mods that were never built for it "
			"can appear here too.";
	}

	std::span<const HomeQuickLink> HomeQuickLinks() noexcept
	{
		return kQuickLinks;
	}

	std::vector<HomeFaqEntry> BuildHomeFaq(
		std::string_view a_toggleKeyName)
	{
		std::string toggleAnswer{ "Press " };
		toggleAnswer.append(a_toggleKeyName);
		toggleAnswer.append(
			" to open or close DearModdingUI. You can change this key on "
			"the Settings page.");
		return {
			{
				"How do I open the menu?",
				std::move(toggleAnswer)
			},
			{
				"Where are settings stored?",
				"Host settings are stored in "
				"Data/F4SE/Plugins/DearModdingUI.toml."
			},
			{
				"Why is a mod page missing or grayed out?",
				"Open the Health page to see whether the host or that mod "
				"reported a problem."
			},
			{
				"Does this replace a mod's own menu?",
				"No. Mods register pages and DearModdingUI draws them in the "
				"shared overlay."
			}
		};
	}

	std::string BuildHomeHealthSummary(
		std::span<const HealthSnapshot> a_subsystems,
		size_t a_clientsNeedingAttention,
		HealthClock::time_point a_now)
	{
		if (a_subsystems.empty() && a_clientsNeedingAttention == 0)
			return "Host health not observed yet";

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
			return "All systems ready";

		char summary[192]{};
		if (subsystemsNeedingAttention != 0 &&
			a_clientsNeedingAttention != 0)
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu host subsystem%s and %zu mod%s need attention",
				subsystemsNeedingAttention,
				subsystemsNeedingAttention == 1 ? "" : "s",
				a_clientsNeedingAttention,
				a_clientsNeedingAttention == 1 ? "" : "s");
		}
		else if (subsystemsStarting != 0 &&
			a_clientsNeedingAttention != 0)
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu host subsystem%s starting; %zu mod%s need%s attention",
				subsystemsStarting,
				subsystemsStarting == 1 ? "" : "s",
				a_clientsNeedingAttention,
				a_clientsNeedingAttention == 1 ? "" : "s",
				a_clientsNeedingAttention == 1 ? "s" : "");
		}
		else if (subsystemsNeedingAttention != 0)
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu host subsystem%s need%s attention",
				subsystemsNeedingAttention,
				subsystemsNeedingAttention == 1 ? "" : "s",
				subsystemsNeedingAttention == 1 ? "s" : "");
		}
		else if (subsystemsStarting != 0)
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu host subsystem%s starting",
				subsystemsStarting,
				subsystemsStarting == 1 ? "" : "s");
		}
		else
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu mod%s need%s attention",
				a_clientsNeedingAttention,
				a_clientsNeedingAttention == 1 ? "" : "s",
				a_clientsNeedingAttention == 1 ? "s" : "");
		}
		return summary;
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
