#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <concepts>
#include <string_view>
#include <type_traits>

namespace DearModdingUI::MCM
{
	template <class Page>
		requires std::same_as<std::remove_const_t<Page>, dmui::SettingsPage>
	[[nodiscard]] auto FindSettingDescriptor(Page& a_page, std::string_view a_id) noexcept
		-> decltype(&a_page.groups.front().settings.front())
	{
		for (auto& group : a_page.groups)
		{
			for (auto& setting : group.settings)
			{
				if (setting.id == a_id)
					return &setting;
			}
		}
		return nullptr;
	}

	template <class Page>
		requires std::same_as<std::remove_const_t<Page>, dmui::SettingsPage>
	[[nodiscard]] auto FindActionRow(Page& a_page, std::string_view a_id) noexcept
		-> decltype(&a_page.groups.front().actionRows.front())
	{
		for (auto& group : a_page.groups)
		{
			for (auto& action : group.actionRows)
			{
				if (action.id == a_id)
					return &action;
			}
		}
		return nullptr;
	}

}
