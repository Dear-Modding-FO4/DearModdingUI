#include <Platform/input/EngineFocus.h>

#include <Windows.h>

#include <atomic>

namespace DearModdingUI::EngineFocus
{
	namespace
	{
		std::atomic<Source> s_source{ nullptr };
	}

	void SetSource(Source a_source) noexcept
	{
		s_source.store(a_source, std::memory_order_release);
	}

	uint32_t UiThread() noexcept
	{
		const auto source = s_source.load(std::memory_order_acquire);
		return source ? source().uiThread : 0;
	}

	bool OnUiThread() noexcept
	{
		const auto thread = UiThread();
		return thread && thread == GetCurrentThreadId();
	}

	bool Polling() noexcept
	{
		const auto source = s_source.load(std::memory_order_acquire);
		return source && source().pollingEnabled;
	}

	uint32_t ForegroundThread() noexcept
	{
		const auto foreground = GetForegroundWindow();
		return foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
	}

	Snapshot Read() noexcept
	{
		const auto source = s_source.load(std::memory_order_acquire);
		return source ? source() : Snapshot{};
	}

	bool Active() noexcept
	{
		const auto source = s_source.load(std::memory_order_acquire);
		if (!source)
			return false;
		const auto foreground = GetForegroundWindow();
		return foreground &&
			IsFocused(source(), GetWindowThreadProcessId(foreground, nullptr));
	}
}
