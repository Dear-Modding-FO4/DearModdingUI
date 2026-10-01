#pragma once

#include <DearModdingUI/host/Hotkeys.h>

namespace DearModdingUI
{
	inline constexpr HotkeyChord kMenuDefaultToggleChord{ 0xCF, 0 };
	inline constexpr HotkeyChord kMenuDefaultGamepadToggleChord{
		{ KeyCatalog::kPadBack, KeyCatalog::kPadLB, KeyCatalog::kPadRB }, 0 };

	[[nodiscard]] inline ParsedHotkeyChord ParseMenuToggleChord(
		std::string_view a_value,
		HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept
	{
		const auto parsed = ParseHotkeyChord(a_value);
		const auto gamepad = a_slot == HotkeySlot::kGamepad;
		return parsed.recognized && parsed.chord.FitsSlot(a_slot) &&
				(gamepad || !parsed.chord.IsNone()) ?
			parsed : ParsedHotkeyChord{
				gamepad ? kMenuDefaultGamepadToggleChord : kMenuDefaultToggleChord, false };
	}

	[[nodiscard]] constexpr bool DecideMenuToggle(
		bool a_open,
		bool a_drawingEnabled) noexcept
	{
		return !(a_open && a_drawingEnabled);
	}
}
