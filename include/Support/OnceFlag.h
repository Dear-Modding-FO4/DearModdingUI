#pragma once

#include <atomic>

namespace DearModdingUI::Support
{
	// True for exactly one caller across threads; for log-once diagnostics.
	class OnceFlag
	{
	public:
		[[nodiscard]] bool First() noexcept
		{
			return !m_done.exchange(true, std::memory_order_acq_rel);
		}

	private:
		std::atomic<bool> m_done{ false };
	};
}
