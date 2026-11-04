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
	void PrepareFrame(bool a_modalVisible) noexcept;
	void ApplyNativePosition(float a_x, float a_y) noexcept;
	// Runs after the window procedure; true when the OS cursor was hidden.
	[[nodiscard]] bool ApplySetCursor(void* a_window, uint32_t a_hitTest) noexcept;
	void Shutdown() noexcept;
}
