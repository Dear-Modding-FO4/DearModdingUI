#pragma once

#include <DearModdingUI/API.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <vector>

namespace DearModdingUI
{
	enum class OverlayPageFocus : uint32_t
	{
		kNone,
		kFocused,
		kGranted
	};

	// Enders publish state before End and admission reads it under the lock: no grant outlives it.
	class OverlayFocusState
	{
	public:
		template <class Admit>
			requires std::is_nothrow_invocable_r_v<DMUI_Result, Admit&>
		[[nodiscard]] DMUI_Result Request(DMUI_PageHandle a_page, Admit&& a_admit) noexcept
		{
			const std::scoped_lock lock{ m_mutex };
			const auto holder = m_holder.load(std::memory_order_acquire);
			if (holder == a_page)
				return DMUI_RESULT_OK;
			const auto admitted = a_admit();
			if (admitted != DMUI_RESULT_OK)
				return admitted;
			return holder == DMUI_INVALID_PAGE_HANDLE ? GrantLocked(a_page) : DMUI_RESULT_BUSY;
		}

		template <class StillValid>
			requires std::is_nothrow_invocable_r_v<bool, StillValid&>
		void EndPageUnless(
			DMUI_PageHandle a_page,
			DMUI_OverlayFocusEndReason a_reason,
			StillValid&& a_stillValid) noexcept
		{
			const std::scoped_lock lock{ m_mutex };
			if (m_holder.load(std::memory_order_acquire) == a_page && !a_stillValid())
				EndLocked(a_reason);
		}

		void End(DMUI_OverlayFocusEndReason a_reason) noexcept;
		void EndPage(DMUI_PageHandle a_page, DMUI_OverlayFocusEndReason a_reason) noexcept;
		[[nodiscard]] DMUI_OverlayFocusInfo Query(DMUI_PageHandle a_page) const noexcept;
		[[nodiscard]] bool IsActive() const noexcept;
		// Render thread: kGranted is reported once per grant so the overlay takes window focus.
		[[nodiscard]] OverlayPageFocus Observe(DMUI_PageHandle a_page) noexcept;

	private:
		struct PageRecord
		{
			DMUI_PageHandle page{ DMUI_INVALID_PAGE_HANDLE };
			uint64_t generation{};
			DMUI_OverlayFocusEndReason endReason{ DMUI_OVERLAY_FOCUS_END_NONE };
		};

		[[nodiscard]] DMUI_Result GrantLocked(DMUI_PageHandle a_page) noexcept;
		void EndLocked(DMUI_OverlayFocusEndReason a_reason) noexcept;
		[[nodiscard]] const PageRecord* FindRecord(DMUI_PageHandle a_page) const noexcept;

		mutable std::mutex m_mutex;
		std::atomic<DMUI_PageHandle> m_holder{ DMUI_INVALID_PAGE_HANDLE };
		bool m_granted{};
		std::vector<PageRecord> m_records;
	};
}
