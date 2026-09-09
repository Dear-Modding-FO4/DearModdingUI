#pragma once

#include <atomic>
#include <type_traits>
#include <utility>

namespace Addictol::Support
{
	// The owner must outlive its queued work.
	class CoalescedTask
	{
	public:
		template <class Submit, class Callback>
		[[nodiscard]] bool TrySubmit(Submit&& a_submit, Callback a_callback)
		{
			static_assert(std::is_nothrow_invocable_v<Callback>);
			if (m_pending.exchange(true, std::memory_order_acq_rel))
				return false;

			struct Rollback
			{
				std::atomic<bool>& pending;
				bool submitted{};
				~Rollback()
				{
					if (!submitted)
						pending.store(false, std::memory_order_release);
				}
			} rollback{ m_pending };
			a_submit([this, a_callback]() noexcept {
				a_callback();
				m_pending.store(false, std::memory_order_release);
			});
			rollback.submitted = true;
			return true;
		}

	private:
		std::atomic<bool> m_pending{ false };
	};
}
