#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace DearModdingUI
{
	[[nodiscard]] constexpr char AsciiUpper(char a_character) noexcept
	{
		return a_character >= 'a' && a_character <= 'z' ?
			static_cast<char>(a_character - ('a' - 'A')) :
			a_character;
	}

	[[nodiscard]] constexpr bool EqualsIgnoringCase(
		std::string_view a_left,
		std::string_view a_right) noexcept
	{
		if (a_left.size() != a_right.size())
			return false;
		for (size_t index = 0; index < a_left.size(); ++index)
		{
			if (AsciiUpper(a_left[index]) != AsciiUpper(a_right[index]))
				return false;
		}
		return true;
	}

	namespace KeyCatalog
	{
		// F4SE InputMap codes keep keyboard, mouse, and gamepad identities disjoint.
		inline constexpr int32_t kKeyboardKeyCount = 256;
		inline constexpr int32_t kMouseButtonOffset = 256;
		inline constexpr int32_t kMouseButtonCount = 8;
		inline constexpr int32_t kMouseWheelOffset = 264;
		inline constexpr int32_t kMouseWheelDirectionCount = 2;
		inline constexpr int32_t kGamepadButtonOffset = 266;
		inline constexpr int32_t kGamepadButtonCount = 16;
		inline constexpr int32_t kMaximumMacroCode = 282;

		struct Entry
		{
			uint32_t code;
			std::string_view token;
			std::string_view label;
		};

		inline constexpr std::array kKeys{
			Entry{ 0x01, "Escape", "Esc" },
			Entry{ 0x02, "1", "1" },
			Entry{ 0x03, "2", "2" },
			Entry{ 0x04, "3", "3" },
			Entry{ 0x05, "4", "4" },
			Entry{ 0x06, "5", "5" },
			Entry{ 0x07, "6", "6" },
			Entry{ 0x08, "7", "7" },
			Entry{ 0x09, "8", "8" },
			Entry{ 0x0A, "9", "9" },
			Entry{ 0x0B, "0", "0" },
			Entry{ 0x0C, "Minus", "-" },
			Entry{ 0x0D, "Equals", "=" },
			Entry{ 0x0E, "Backspace", "Backspace" },
			Entry{ 0x0F, "Tab", "Tab" },
			Entry{ 0x10, "Q", "Q" },
			Entry{ 0x11, "W", "W" },
			Entry{ 0x12, "E", "E" },
			Entry{ 0x13, "R", "R" },
			Entry{ 0x14, "T", "T" },
			Entry{ 0x15, "Y", "Y" },
			Entry{ 0x16, "U", "U" },
			Entry{ 0x17, "I", "I" },
			Entry{ 0x18, "O", "O" },
			Entry{ 0x19, "P", "P" },
			Entry{ 0x1A, "LeftBracket", "[" },
			Entry{ 0x1B, "RightBracket", "]" },
			Entry{ 0x1C, "Enter", "Enter" },
			Entry{ 0x1D, "LeftCtrl", "Left Ctrl" },
			Entry{ 0x1E, "A", "A" },
			Entry{ 0x1F, "S", "S" },
			Entry{ 0x20, "D", "D" },
			Entry{ 0x21, "F", "F" },
			Entry{ 0x22, "G", "G" },
			Entry{ 0x23, "H", "H" },
			Entry{ 0x24, "J", "J" },
			Entry{ 0x25, "K", "K" },
			Entry{ 0x26, "L", "L" },
			Entry{ 0x27, "Semicolon", ";" },
			Entry{ 0x28, "Apostrophe", "'" },
			Entry{ 0x29, "Grave", "`" },
			Entry{ 0x2A, "LeftShift", "Left Shift" },
			Entry{ 0x2B, "Backslash", "\\" },
			Entry{ 0x2C, "Z", "Z" },
			Entry{ 0x2D, "X", "X" },
			Entry{ 0x2E, "C", "C" },
			Entry{ 0x2F, "V", "V" },
			Entry{ 0x30, "B", "B" },
			Entry{ 0x31, "N", "N" },
			Entry{ 0x32, "M", "M" },
			Entry{ 0x33, "Comma", "," },
			Entry{ 0x34, "Period", "." },
			Entry{ 0x35, "Slash", "/" },
			Entry{ 0x36, "RightShift", "Right Shift" },
			Entry{ 0x37, "NumpadMultiply", "Numpad *" },
			Entry{ 0x38, "LeftAlt", "Left Alt" },
			Entry{ 0x39, "Space", "Space" },
			Entry{ 0x3A, "CapsLock", "Caps Lock" },
			Entry{ 0x3B, "F1", "F1" },
			Entry{ 0x3C, "F2", "F2" },
			Entry{ 0x3D, "F3", "F3" },
			Entry{ 0x3E, "F4", "F4" },
			Entry{ 0x3F, "F5", "F5" },
			Entry{ 0x40, "F6", "F6" },
			Entry{ 0x41, "F7", "F7" },
			Entry{ 0x42, "F8", "F8" },
			Entry{ 0x43, "F9", "F9" },
			Entry{ 0x44, "F10", "F10" },
			Entry{ 0x45, "NumLock", "Num Lock" },
			Entry{ 0x46, "ScrollLock", "Scroll Lock" },
			Entry{ 0x47, "Numpad7", "Numpad 7" },
			Entry{ 0x48, "Numpad8", "Numpad 8" },
			Entry{ 0x49, "Numpad9", "Numpad 9" },
			Entry{ 0x4A, "NumpadSubtract", "Numpad -" },
			Entry{ 0x4B, "Numpad4", "Numpad 4" },
			Entry{ 0x4C, "Numpad5", "Numpad 5" },
			Entry{ 0x4D, "Numpad6", "Numpad 6" },
			Entry{ 0x4E, "NumpadAdd", "Numpad +" },
			Entry{ 0x4F, "Numpad1", "Numpad 1" },
			Entry{ 0x50, "Numpad2", "Numpad 2" },
			Entry{ 0x51, "Numpad3", "Numpad 3" },
			Entry{ 0x52, "Numpad0", "Numpad 0" },
			Entry{ 0x53, "NumpadDecimal", "Numpad ." },
			Entry{ 0x56, "Oem102", "OEM 102" },
			Entry{ 0x57, "F11", "F11" },
			Entry{ 0x58, "F12", "F12" },
			Entry{ 0x64, "F13", "F13" },
			Entry{ 0x65, "F14", "F14" },
			Entry{ 0x66, "F15", "F15" },
			Entry{ 0x70, "Kana", "Kana" },
			Entry{ 0x73, "AbntC1", "ABNT C1" },
			Entry{ 0x79, "Convert", "Convert" },
			Entry{ 0x7B, "NoConvert", "No Convert" },
			Entry{ 0x7D, "Yen", "Yen" },
			Entry{ 0x7E, "AbntC2", "ABNT C2" },
			Entry{ 0x8D, "NumpadEquals", "Numpad =" },
			Entry{ 0x90, "PreviousTrack", "Previous Track" },
			Entry{ 0x91, "At", "At" },
			Entry{ 0x92, "Colon", "Colon" },
			Entry{ 0x93, "Underline", "Underline" },
			Entry{ 0x94, "Kanji", "Kanji" },
			Entry{ 0x95, "Stop", "Stop" },
			Entry{ 0x96, "Ax", "AX" },
			Entry{ 0x97, "Unlabeled", "Unlabeled" },
			Entry{ 0x99, "NextTrack", "Next Track" },
			Entry{ 0x9C, "NumpadEnter", "Numpad Enter" },
			Entry{ 0x9D, "RightCtrl", "Right Ctrl" },
			Entry{ 0xA0, "Mute", "Mute" },
			Entry{ 0xA1, "Calculator", "Calculator" },
			Entry{ 0xA2, "PlayPause", "Play/Pause" },
			Entry{ 0xA4, "MediaStop", "Media Stop" },
			Entry{ 0xAE, "VolumeDown", "Volume Down" },
			Entry{ 0xB0, "VolumeUp", "Volume Up" },
			Entry{ 0xB2, "WebHome", "Web Home" },
			Entry{ 0xB3, "NumpadComma", "Numpad ," },
			Entry{ 0xB5, "NumpadDivide", "Numpad /" },
			Entry{ 0xB7, "PrintScreen", "SysRq" },
			Entry{ 0xB8, "RightAlt", "Right Alt" },
			Entry{ 0xC5, "Pause", "Pause" },
			Entry{ 0xC7, "Home", "Home" },
			Entry{ 0xC8, "Up", "Up" },
			Entry{ 0xC9, "PageUp", "Page Up" },
			Entry{ 0xCB, "Left", "Left" },
			Entry{ 0xCD, "Right", "Right" },
			Entry{ 0xCF, "End", "End" },
			Entry{ 0xD0, "Down", "Down" },
			Entry{ 0xD1, "PageDown", "Page Down" },
			Entry{ 0xD2, "Insert", "Insert" },
			Entry{ 0xD3, "Delete", "Delete" },
			Entry{ 0xDB, "LeftWin", "Left Windows" },
			Entry{ 0xDC, "RightWin", "Right Windows" },
			Entry{ 0xDD, "Apps", "Menu" },
			Entry{ 0xDE, "Power", "Power" },
			Entry{ 0xDF, "Sleep", "Sleep" },
			Entry{ 0xE3, "Wake", "Wake" },
			Entry{ 0xE5, "WebSearch", "Web Search" },
			Entry{ 0xE6, "WebFavorites", "Web Favorites" },
			Entry{ 0xE7, "WebRefresh", "Web Refresh" },
			Entry{ 0xE8, "WebStop", "Web Stop" },
			Entry{ 0xE9, "WebForward", "Web Forward" },
			Entry{ 0xEA, "WebBack", "Web Back" },
			Entry{ 0xEB, "MyComputer", "My Computer" },
			Entry{ 0xEC, "Mail", "Mail" },
			Entry{ 0xED, "MediaSelect", "Media Select" },
			Entry{ 256, "Mouse1", "Mouse 1" },
			Entry{ 257, "Mouse2", "Mouse 2" },
			Entry{ 258, "Mouse3", "Mouse 3" },
			Entry{ 259, "Mouse4", "Mouse 4" },
			Entry{ 260, "Mouse5", "Mouse 5" },
			Entry{ 261, "Mouse6", "Mouse 6" },
			Entry{ 262, "Mouse7", "Mouse 7" },
			Entry{ 263, "Mouse8", "Mouse 8" },
			Entry{ 264, "WheelUp", "Mouse Wheel Up" },
			Entry{ 265, "WheelDown", "Mouse Wheel Down" },
			Entry{ 266, "PadUp", "D-Pad Up" },
			Entry{ 267, "PadDown", "D-Pad Down" },
			Entry{ 268, "PadLeft", "D-Pad Left" },
			Entry{ 269, "PadRight", "D-Pad Right" },
			Entry{ 270, "PadStart", "Start" },
			Entry{ 271, "PadBack", "Back" },
			Entry{ 272, "PadLS", "Left Stick" },
			Entry{ 273, "PadRS", "Right Stick" },
			Entry{ 274, "PadLB", "Left Bumper" },
			Entry{ 275, "PadRB", "Right Bumper" },
			Entry{ 276, "PadA", "A" },
			Entry{ 277, "PadB", "B" },
			Entry{ 278, "PadX", "X" },
			Entry{ 279, "PadY", "Y" },
			Entry{ 280, "PadLT", "Left Trigger" },
			Entry{ 281, "PadRT", "Right Trigger" }
		};

		[[nodiscard]] constexpr const Entry* Find(uint32_t a_code) noexcept
		{
			for (const auto& key : kKeys)
			{
				if (key.code == a_code)
					return &key;
			}
			return nullptr;
		}

		[[nodiscard]] constexpr std::optional<uint32_t> Parse(
			std::string_view a_token) noexcept
		{
			for (const auto& key : kKeys)
			{
				if (EqualsIgnoringCase(a_token, key.token))
					return key.code;
			}
			if (EqualsIgnoringCase(a_token, "PgUp"))
				return 0xC9;
			if (EqualsIgnoringCase(a_token, "PgDn"))
				return 0xD1;
			if (EqualsIgnoringCase(a_token, "Esc"))
				return 0x01;
			return std::nullopt;
		}

		[[nodiscard]] constexpr std::string_view Token(uint32_t a_code) noexcept
		{
			const auto* key = Find(a_code);
			return key ? key->token : std::string_view{};
		}

		[[nodiscard]] constexpr std::string_view Label(uint32_t a_code) noexcept
		{
			const auto* key = Find(a_code);
			return key ? key->label : std::string_view{};
		}
	}
}
