#pragma once

#include <DearModdingUI/host/Hotkeys.h>

namespace DearModdingUI
{
	inline constexpr HotkeyChord kMenuDefaultToggleChord{ 0xCF, 0 };

	[[nodiscard]] inline ParsedHotkeyChord ParseMenuToggleChord(
		std::string_view a_value) noexcept
	{
		const auto parsed = ParseHotkeyChord(a_value);
		return parsed.recognized && !parsed.chord.IsNone() ?
			parsed : ParsedHotkeyChord{ kMenuDefaultToggleChord, false };
	}

	struct MenuToggleDecision
	{
		bool matched{ false };
		bool open{ false };
	};

	[[nodiscard]] constexpr MenuToggleDecision DecideMenuToggle(
		HotkeyChord a_chord,
		HotkeyChord a_toggleChord,
		bool a_open,
		bool a_drawingEnabled) noexcept
	{
		if (a_chord != a_toggleChord)
			return { false, a_open };
		return { true, !(a_open && a_drawingEnabled) };
	}
}
