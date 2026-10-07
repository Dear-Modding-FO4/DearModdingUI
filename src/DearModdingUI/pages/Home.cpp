#include <DearModdingUI/pages/Home.h>
#include <DearModdingUI/host/LocalizationStrings.h>

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
				"https://www.nexusmods.com/fallout4/mods/108874",
				{},
				{},
				true }
		};
	}

	std::string_view HomeAboutText() noexcept
	{
		return lsHomeAboutText;
	}

	std::span<const HomeQuickLink> HomeQuickLinks() noexcept
	{
		return kQuickLinks;
	}

	std::vector<HomeFaqEntry> BuildHomeFaq(
		std::string_view a_toggleKeyName)
	{
		std::string toggleAnswer{ lsPress.GetValue() };
		toggleAnswer.append(" ").append(a_toggleKeyName);
		toggleAnswer.append(" ").append(lsFaqA1.GetValue());
		return {
			{
				lsFaq1,
				std::move(toggleAnswer)
			},
			{
				lsFaq2, lsFaqA2
			},
			{
				lsFaq3, lsFaqA3
			},
			{
				lsFaq4, lsFaqA4
			}
		};
	}

	std::string BuildHomeHealthSummary(
		std::span<const HealthSnapshot> a_subsystems,
		size_t a_clientsNeedingAttention,
		HealthClock::time_point a_now)
	{
		if (a_subsystems.empty() && a_clientsNeedingAttention == 0)
			return lsHostHealthNotObservedYet;

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
			return lsAllSystemsReady;

		char summary[192]{};
		if (subsystemsNeedingAttention != 0 &&
			a_clientsNeedingAttention != 0)
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu %s %zu %s",
				subsystemsNeedingAttention,
				subsystemsNeedingAttention == 1 ? lsSystemsPartMsgFailed1_1.GetValue().c_str() : lsSystemsPartMsgFailed1_2.GetValue().c_str(),
				a_clientsNeedingAttention,
				a_clientsNeedingAttention == 1 ? lsSystemsPartMsgFailed2_1.GetValue().c_str() : lsSystemsPartMsgFailed2_2.GetValue().c_str());
		}
		else if (subsystemsStarting != 0 &&
			a_clientsNeedingAttention != 0)
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu %s %zu %s",
				subsystemsStarting,
				subsystemsStarting == 1 ? lsSystemsPartMsgFailed3_1.GetValue().c_str() : lsSystemsPartMsgFailed4_1.GetValue().c_str(),
				a_clientsNeedingAttention,
				a_clientsNeedingAttention == 1 ? lsSystemsPartMsgFailed2_1.GetValue().c_str() : lsSystemsPartMsgFailed2_2.GetValue().c_str());
		}
		else if (subsystemsNeedingAttention != 0)
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu %s",
				subsystemsNeedingAttention,
				subsystemsNeedingAttention == 1 ? lsSystemsPartMsgFailed5_1.GetValue().c_str() : lsSystemsPartMsgFailed6_1.GetValue().c_str());
		}
		else if (subsystemsStarting != 0)
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu %s",
				subsystemsStarting,
				subsystemsStarting == 1 ? lsSystemsPartMsgFailed3_1.GetValue().c_str() : lsSystemsPartMsgFailed4_1.GetValue().c_str());
		}
		else
		{
			std::snprintf(
				summary,
				sizeof(summary),
				"%zu %s",
				a_clientsNeedingAttention,
				a_clientsNeedingAttention == 1 ? lsSystemsPartMsgFailed2_1.GetValue().c_str() : lsSystemsPartMsgFailed2_2.GetValue().c_str());
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
