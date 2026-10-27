#pragma once

#include <imgui/imgui.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace DearModdingUI
{
	struct ShortcutChord
	{
		ImGuiKeyChord chord{};
		// The same chord in host hotkey terms, for conflict checks.
		uint32_t keyCode{};
		uint32_t modifiers{};
	};

	// Keyboard chords in the host hotkey grammar. Gamepad and mouse chords,
	// multi-key chords, Escape (reserved for dismissal), and keys ImGui has
	// no name for yield nothing.
	[[nodiscard]] std::optional<ShortcutChord> ParseShortcutChord(
		std::string_view a_text) noexcept;

	// True on the press frame unless a host hotkey would act on the chord.
	// A nonzero owner routes the chord to that item instead of the window.
	[[nodiscard]] bool ShortcutPressed(
		const ShortcutChord& a_chord,
		ImGuiInputFlags a_flags,
		ImGuiID a_owner = 0) noexcept;
}
