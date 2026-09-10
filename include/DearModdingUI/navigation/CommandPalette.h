#pragma once

#include <DearModdingUI/navigation/Navigation.h>

#include <string>

namespace DearModdingUI
{
	struct CommandPaletteState
	{
		std::string query;
		size_t selection{ 0 };
		bool openRequested{ false };
		bool focusRequested{ false };
		bool visible{ false };

		void RequestOpen() noexcept
		{
			openRequested = true;
		}
	};

	[[nodiscard]] const NavigationSearchEntry* DrawCommandPalette(
		const NavigationModel& a_model,
		const ClientSelectionState& a_selection,
		CommandPaletteState& a_state) noexcept;
}
