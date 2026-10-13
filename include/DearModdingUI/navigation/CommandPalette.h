#pragma once

#include <DearModdingUI/navigation/Navigation.h>
#include <Support/Revision.h>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI
{
	struct CommandPaletteRow
	{
		NavigationSearchHit hit;
		std::string id;
		std::string text;
	};

	// Empty-query rows list recent pages, which may change while open.
	struct CommandPaletteRowsKey
	{
		struct View
		{
			std::string_view query;
			std::span<const DMUI_PageHandle> recentPages;
		};

		explicit CommandPaletteRowsKey(View a_view) :
			query(a_view.query),
			recentPages(a_view.recentPages.begin(), a_view.recentPages.end())
		{}

		[[nodiscard]] bool operator==(const View& a_view) const noexcept
		{
			return query == a_view.query &&
				std::ranges::equal(recentPages, a_view.recentPages);
		}

		std::string query;
		std::vector<DMUI_PageHandle> recentPages;
	};

	struct CommandPaletteState
	{
		std::string query;
		Addictol::Support::Derived<CommandPaletteRowsKey, std::vector<CommandPaletteRow>> rows;
		size_t selection{ 0 };
		bool openRequested{ false };
		bool focusRequested{ false };
		bool visible{ false };

		void RequestOpen() noexcept
		{
			openRequested = true;
		}
	};

	[[nodiscard]] const char* CommandPaletteHint() noexcept;

	[[nodiscard]] const NavigationSearchEntry* DrawCommandPalette(
		const NavigationModel& a_model,
		const ClientSelectionState& a_selection,
		CommandPaletteState& a_state) noexcept;
}
