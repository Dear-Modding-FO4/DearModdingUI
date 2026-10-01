#pragma once

#include <Support/KeyCatalog.h>

namespace DearModdingUI
{
	struct MenuToggleKey
	{
		uint32_t keyCode{ 0 };
		bool recognized{ false };
	};

	inline constexpr uint32_t kMenuDefaultToggleKey{ 0xCF };

	[[nodiscard]] constexpr bool IsHostBindableKey(uint32_t a_keyCode) noexcept
	{
		if (a_keyCode >= KeyCatalog::kKeyboardKeyCount ||
			!KeyCatalog::Find(a_keyCode))
			return false;
		// Escape drives dismissal; standalone modifiers only qualify chords.
		switch (a_keyCode)
		{
		case 0x01:
		case 0x1D:
		case 0x9D:
		case 0x2A:
		case 0x36:
		case 0x38:
		case 0xB8:
			return false;
		default:
			return true;
		}
	}

	[[nodiscard]] constexpr MenuToggleKey ParseMenuToggleKey(
		std::string_view a_name) noexcept
	{
		const auto code = KeyCatalog::Parse(a_name);
		if (code && IsHostBindableKey(*code))
			return { *code, true };
		return { kMenuDefaultToggleKey, false };
	}

	[[nodiscard]] constexpr std::string_view MenuToggleKeyName(
		uint32_t a_keyCode) noexcept
	{
		const auto token = KeyCatalog::Token(a_keyCode);
		return token.empty() ? std::string_view{ "Unknown" } : token;
	}

	struct MenuToggleDecision
	{
		bool matched{ false };
		bool open{ false };
	};

	[[nodiscard]] constexpr MenuToggleDecision DecideMenuToggle(
		uint32_t a_keyCode,
		uint32_t a_toggleKey,
		bool a_open,
		bool a_drawingEnabled) noexcept
	{
		if (a_keyCode != a_toggleKey)
			return { false, a_open };
		return { true, !(a_open && a_drawingEnabled) };
	}
}
