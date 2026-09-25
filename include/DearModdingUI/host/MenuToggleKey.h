#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace DearModdingUI
{
	using namespace std::literals;

	struct MenuToggleKey
	{
		uint32_t virtualKey{ 0 };
		bool recognized{ false };
	};

	inline constexpr uint32_t kMenuDefaultToggleKey{ 0x23 };

	struct MenuKeyName
	{
		std::string_view name;
		uint32_t virtualKey;
	};

	inline constexpr std::array kMenuToggleKeys{
		MenuKeyName{ "F1"sv, 0x70 },
		MenuKeyName{ "F2"sv, 0x71 },
		MenuKeyName{ "F3"sv, 0x72 },
		MenuKeyName{ "F4"sv, 0x73 },
		MenuKeyName{ "F5"sv, 0x74 },
		MenuKeyName{ "F6"sv, 0x75 },
		MenuKeyName{ "F7"sv, 0x76 },
		MenuKeyName{ "F8"sv, 0x77 },
		MenuKeyName{ "F9"sv, 0x78 },
		MenuKeyName{ "F10"sv, 0x79 },
		MenuKeyName{ "F11"sv, 0x7A },
		MenuKeyName{ "F12"sv, 0x7B },
		MenuKeyName{ "Home"sv, 0x24 },
		MenuKeyName{ "End"sv, 0x23 },
		MenuKeyName{ "PageUp"sv, 0x21 },
		MenuKeyName{ "PageDown"sv, 0x22 },
		MenuKeyName{ "Insert"sv, 0x2D },
		MenuKeyName{ "Delete"sv, 0x2E }
	};

	// Accepted when parsing only, so pickers list each key once.
	inline constexpr std::array kMenuToggleKeyAliases{
		MenuKeyName{ "PgUp"sv, 0x21 },
		MenuKeyName{ "PgDn"sv, 0x22 }
	};

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

	[[nodiscard]] constexpr MenuToggleKey ParseMenuToggleKey(
		std::string_view a_name) noexcept
	{
		for (const auto& key : kMenuToggleKeys)
		{
			if (EqualsIgnoringCase(a_name, key.name))
				return { key.virtualKey, true };
		}
		for (const auto& key : kMenuToggleKeyAliases)
		{
			if (EqualsIgnoringCase(a_name, key.name))
				return { key.virtualKey, true };
		}
		return { kMenuDefaultToggleKey, false };
	}

	[[nodiscard]] constexpr std::string_view MenuToggleKeyName(
		uint32_t a_virtualKey) noexcept
	{
		for (const auto& key : kMenuToggleKeys)
		{
			if (key.virtualKey == a_virtualKey)
				return key.name;
		}
		return "Unknown"sv;
	}

	struct MenuToggleDecision
	{
		bool matched{ false };
		bool open{ false };
	};

	[[nodiscard]] constexpr MenuToggleDecision DecideMenuToggle(
		uint32_t a_virtualKey,
		uint32_t a_toggleKey,
		bool a_open,
		bool a_drawingEnabled) noexcept
	{
		if (a_virtualKey != a_toggleKey)
			return { false, a_open };
		return { true, !(a_open && a_drawingEnabled) };
	}
}
