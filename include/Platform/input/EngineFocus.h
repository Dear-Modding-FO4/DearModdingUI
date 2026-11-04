#pragma once

#include <cstdint>

namespace DearModdingUI::EngineFocus
{
	struct Snapshot
	{
		bool pollingEnabled{ false };
		uint32_t uiThread{ 0 };
	};

	using Source = Snapshot (*)() noexcept;

	// The preview supplies its own window thread; the host reads the engine.
	void SetSource(Source a_source) noexcept;

	// Foreground-thread match keeps wrapper windows on the UI thread focused.
	[[nodiscard]] constexpr bool IsFocused(Snapshot a_state, uint32_t a_foregroundThread) noexcept
	{
		return a_state.pollingEnabled && a_state.uiThread && a_foregroundThread == a_state.uiThread;
	}

	[[nodiscard]] bool Active() noexcept;
	// Zero when no source or no foreground window.
	[[nodiscard]] uint32_t ForegroundThread() noexcept;
	[[nodiscard]] Snapshot Read() noexcept;
	// The engine's own input-polling flag, for diagnostics.
	[[nodiscard]] bool Polling() noexcept;

	// Zero until a source reports a window thread.
	[[nodiscard]] uint32_t UiThread() noexcept;
	[[nodiscard]] bool OnUiThread() noexcept;
}
