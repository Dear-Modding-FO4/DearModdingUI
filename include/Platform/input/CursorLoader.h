#pragma once

#include <cstdint>

namespace DearModdingUI::CursorLoader
{
	enum class Source
	{
		kSoftware,
		kGame
	};

	void Initialize(void* a_window, Source a_source = Source::kSoftware) noexcept;
	[[nodiscard]] bool HasFocus() noexcept;
	void PrepareFrame(bool a_modalVisible) noexcept;
	void ApplyNativePosition(float a_x, float a_y) noexcept;
	[[nodiscard]] bool HandleWindowMessage(
		void* a_window,
		uint32_t a_message,
		uint64_t a_lparam) noexcept;
	void Shutdown() noexcept;
}
