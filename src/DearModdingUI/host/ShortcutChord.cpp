#include <DearModdingUI/host/ShortcutChord.h>

#include <DearModdingUI/host/Hotkeys.h>

#include <imgui/imgui_internal.h>

#include <array>

namespace DearModdingUI
{
	namespace
	{
		struct NamedKey
		{
			std::string_view token;
			ImGuiKey key;
		};

		// Letters, digits, and F keys are computed; the rest are named here.
		constexpr std::array kNamedKeys{
			NamedKey{ "Backspace", ImGuiKey_Backspace }, NamedKey{ "Tab", ImGuiKey_Tab },
			NamedKey{ "Enter", ImGuiKey_Enter }, NamedKey{ "Space", ImGuiKey_Space },
			NamedKey{ "Minus", ImGuiKey_Minus }, NamedKey{ "Equals", ImGuiKey_Equal },
			NamedKey{ "LeftBracket", ImGuiKey_LeftBracket },
			NamedKey{ "RightBracket", ImGuiKey_RightBracket },
			NamedKey{ "Semicolon", ImGuiKey_Semicolon },
			NamedKey{ "Apostrophe", ImGuiKey_Apostrophe },
			NamedKey{ "Grave", ImGuiKey_GraveAccent },
			NamedKey{ "Backslash", ImGuiKey_Backslash }, NamedKey{ "Comma", ImGuiKey_Comma },
			NamedKey{ "Period", ImGuiKey_Period }, NamedKey{ "Slash", ImGuiKey_Slash },
			NamedKey{ "CapsLock", ImGuiKey_CapsLock },
			NamedKey{ "ScrollLock", ImGuiKey_ScrollLock },
			NamedKey{ "NumLock", ImGuiKey_NumLock },
			NamedKey{ "PrintScreen", ImGuiKey_PrintScreen },
			NamedKey{ "Pause", ImGuiKey_Pause }, NamedKey{ "Apps", ImGuiKey_Menu },
			NamedKey{ "Home", ImGuiKey_Home }, NamedKey{ "End", ImGuiKey_End },
			NamedKey{ "PageUp", ImGuiKey_PageUp }, NamedKey{ "PageDown", ImGuiKey_PageDown },
			NamedKey{ "Insert", ImGuiKey_Insert }, NamedKey{ "Delete", ImGuiKey_Delete },
			NamedKey{ "Up", ImGuiKey_UpArrow }, NamedKey{ "Down", ImGuiKey_DownArrow },
			NamedKey{ "Left", ImGuiKey_LeftArrow }, NamedKey{ "Right", ImGuiKey_RightArrow },
			NamedKey{ "NumpadMultiply", ImGuiKey_KeypadMultiply },
			NamedKey{ "NumpadSubtract", ImGuiKey_KeypadSubtract },
			NamedKey{ "NumpadAdd", ImGuiKey_KeypadAdd },
			NamedKey{ "NumpadDecimal", ImGuiKey_KeypadDecimal },
			NamedKey{ "NumpadDivide", ImGuiKey_KeypadDivide },
			NamedKey{ "NumpadEnter", ImGuiKey_KeypadEnter },
			NamedKey{ "NumpadEquals", ImGuiKey_KeypadEqual },
			NamedKey{ "Oem102", ImGuiKey_Oem102 }
		};

		[[nodiscard]] ImGuiKey KeyFor(std::string_view a_token) noexcept
		{
			if (a_token.size() == 1)
			{
				const auto character = a_token[0];
				if (character >= 'A' && character <= 'Z')
					return static_cast<ImGuiKey>(ImGuiKey_A + (character - 'A'));
				if (character >= '0' && character <= '9')
					return static_cast<ImGuiKey>(ImGuiKey_0 + (character - '0'));
			}
			if (a_token.size() >= 2 && a_token.size() <= 3 && a_token[0] == 'F')
			{
				int number{};
				for (const auto character : a_token.substr(1))
				{
					if (character < '0' || character > '9')
						return ImGuiKey_None;
					number = number * 10 + (character - '0');
				}
				if (number >= 1 && number <= 15)
					return static_cast<ImGuiKey>(ImGuiKey_F1 + (number - 1));
			}
			if (a_token.starts_with("Numpad") && a_token.size() == 7 &&
				a_token[6] >= '0' && a_token[6] <= '9')
				return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (a_token[6] - '0'));
			for (const auto& named : kNamedKeys)
			{
				if (named.token == a_token)
					return named.key;
			}
			return ImGuiKey_None;
		}
	}

	std::optional<ShortcutChord> ParseShortcutChord(std::string_view a_text) noexcept
	{
		const auto parsed = ParseHotkeyChord(a_text);
		const auto& chord = parsed.chord;
		// A chord is one key and optional modifiers; "none" parses to no key.
		if (!parsed.recognized || chord.IsNone() || chord.keys[1] != 0 ||
			HotkeySlotForKey(chord.keys[0]) != HotkeySlot::kKeyboardMouse)
			return std::nullopt;
		const auto* entry = KeyCatalog::Find(chord.keys[0]);
		if (!entry)
			return std::nullopt;
		const auto key = KeyFor(entry->token);
		if (key == ImGuiKey_None)
			return std::nullopt;
		ImGuiKeyChord imgui = key;
		if (chord.modifiers & kHotkeyModifierShift)
			imgui |= ImGuiMod_Shift;
		if (chord.modifiers & kHotkeyModifierControl)
			imgui |= ImGuiMod_Ctrl;
		if (chord.modifiers & kHotkeyModifierAlt)
			imgui |= ImGuiMod_Alt;
		return ShortcutChord{ imgui, chord.keys[0], chord.modifiers };
	}

	bool ShortcutPressed(
		const ShortcutChord& a_chord,
		ImGuiInputFlags a_flags,
		ImGuiID a_owner) noexcept
	{
		// The host acts on these keys first, so a page never sees them.
		if (Hotkeys::WouldActOn(a_chord.keyCode, a_chord.modifiers))
			return false;
		return ImGui::Shortcut(a_chord.chord, a_flags, a_owner);
	}
}
