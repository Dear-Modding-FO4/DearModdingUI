#include <DearModdingUI/host/OverlayFocus.h>

#include <algorithm>
#include <utility>

namespace DearModdingUI
{
	DMUI_Result OverlayFocusState::GrantLocked(DMUI_PageHandle a_page) noexcept
	{
		auto record = std::ranges::find(m_records, a_page, &PageRecord::page);
		if (record == m_records.end())
		{
			try
			{
				record = m_records.insert(m_records.end(), PageRecord{ a_page });
			}
			catch (...)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
		}
		++record->generation;
		record->endReason = DMUI_OVERLAY_FOCUS_END_NONE;
		m_granted = true;
		m_holder.store(a_page, std::memory_order_release);
		return DMUI_RESULT_OK;
	}

	void OverlayFocusState::EndLocked(DMUI_OverlayFocusEndReason a_reason) noexcept
	{
		const auto holder = m_holder.exchange(DMUI_INVALID_PAGE_HANDLE, std::memory_order_acq_rel);
		if (holder == DMUI_INVALID_PAGE_HANDLE)
			return;
		m_granted = false;
		const auto record = std::ranges::find(m_records, holder, &PageRecord::page);
		if (record != m_records.end())
			record->endReason = a_reason;
	}

	const OverlayFocusState::PageRecord* OverlayFocusState::FindRecord(
		DMUI_PageHandle a_page) const noexcept
	{
		const auto record = std::ranges::find(m_records, a_page, &PageRecord::page);
		return record == m_records.end() ? nullptr : &*record;
	}

	void OverlayFocusState::End(DMUI_OverlayFocusEndReason a_reason) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		EndLocked(a_reason);
	}

	void OverlayFocusState::EndPage(
		DMUI_PageHandle a_page,
		DMUI_OverlayFocusEndReason a_reason) noexcept
	{
		EndPageUnless(a_page, a_reason, []() noexcept { return false; });
	}

	DMUI_OverlayFocusInfo OverlayFocusState::Query(DMUI_PageHandle a_page) const noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		const auto* record = FindRecord(a_page);
		return {
			m_holder.load(std::memory_order_acquire) == a_page ? 1u : 0u,
			record ? record->endReason : DMUI_OVERLAY_FOCUS_END_NONE,
			record ? record->generation : 0u
		};
	}

	bool OverlayFocusState::IsActive() const noexcept
	{
		return m_holder.load(std::memory_order_acquire) != DMUI_INVALID_PAGE_HANDLE;
	}

	OverlayPageFocus OverlayFocusState::Observe(DMUI_PageHandle a_page) noexcept
	{
		if (m_holder.load(std::memory_order_acquire) != a_page)
			return OverlayPageFocus::kNone;
		const std::scoped_lock lock{ m_mutex };
		if (m_holder.load(std::memory_order_acquire) != a_page)
			return OverlayPageFocus::kNone;
		return std::exchange(m_granted, false) ? OverlayPageFocus::kGranted : OverlayPageFocus::kFocused;
	}
}
