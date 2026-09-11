#pragma once

#include <cstdint>

namespace DearModdingUI::CursorLoader
{
	void Initialize(void* a_window) noexcept;
	[[nodiscard]] bool HasFocus() noexcept;
	void PrepareFrame(bool a_modalVisible) noexcept;
	[[nodiscard]] bool HandleWindowMessage(
		void* a_window,
		uint32_t a_message,
		uint64_t a_lparam) noexcept;
	void Shutdown() noexcept;
}
