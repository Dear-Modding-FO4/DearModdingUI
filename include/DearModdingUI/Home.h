#pragma once

#include <Support/SubsystemHealth.h>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI
{
	struct HomeQuickLink
	{
		std::string_view label;
		std::string_view url;
		std::string_view note;
		std::string_view iconName;
		bool enabled;
	};

	struct HomeFaqEntry
	{
		std::string_view question;
		std::string answer;
	};

	[[nodiscard]] std::string_view HomeAboutText() noexcept;
	[[nodiscard]] std::span<const HomeQuickLink> HomeQuickLinks() noexcept;
	[[nodiscard]] std::vector<HomeFaqEntry> BuildHomeFaq(
		std::string_view a_toggleKeyName);
	[[nodiscard]] std::string BuildHomeHealthSummary(
		std::span<const HealthSnapshot> a_subsystems,
		size_t a_clientsNeedingAttention);
}
