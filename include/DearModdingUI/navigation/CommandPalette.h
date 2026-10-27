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

	// Actions the client removed or disabled never appear. The revision
	// changes whenever that can differ, so cached rows rebuild.
	struct CommandPaletteActions
	{
		uint64_t revision{};
		bool (*runnable)(DMUI_ActionHandle) noexcept{ nullptr };
	};

	// Empty-query rows list recent pages, which may change while open.
	struct CommandPaletteRowsKey
	{
		struct View
		{
			std::string_view query;
			std::span<const DMUI_PageHandle> recentPages;
			uint64_t actionRevision;
		};

		explicit CommandPaletteRowsKey(View a_view) :
			query(a_view.query),
			recentPages(a_view.recentPages.begin(), a_view.recentPages.end()),
			actionRevision(a_view.actionRevision)
		{}

		[[nodiscard]] bool operator==(const View& a_view) const noexcept
		{
			return query == a_view.query &&
				actionRevision == a_view.actionRevision &&
				std::ranges::equal(recentPages, a_view.recentPages);
		}

		std::string query;
		std::vector<DMUI_PageHandle> recentPages;
		uint64_t actionRevision;
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
		CommandPaletteState& a_state,
		const CommandPaletteActions& a_actions) noexcept;
}
