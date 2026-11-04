#pragma once

#include <cstdint>

namespace DearModdingUI::ThreadMessageTap
{
	// Thread-scoped, so wrapper windows and subclass order cannot hide messages.
	struct Sink
	{
		// A key press about to be translated; true if a hotkey claims it.
		bool (*keyDown)(uint32_t a_virtualKey) noexcept{ nullptr };
		// One typed UTF-16 unit; true if the game must not see it.
		bool (*text)(char16_t a_unit, bool a_claimed) noexcept{ nullptr };
		void (*inputLanguageChanged)() noexcept{ nullptr };
		// Runs after the window procedure has chosen its cursor.
		void (*setCursor)(void* a_window, uint32_t a_hitTest) noexcept{ nullptr };
	};

	// Windows drops a hook with its installing thread; the tapped thread installs.
	[[nodiscard]] bool Install(const Sink& a_sink) noexcept;
	void Uninstall() noexcept;
	[[nodiscard]] bool Installed() noexcept;
	// The tapped thread, or zero before install.
	[[nodiscard]] uint32_t Thread() noexcept;
}
