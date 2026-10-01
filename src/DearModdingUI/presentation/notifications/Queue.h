#pragma once

#include <DearModdingUI/API.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace DearModdingUI::PresentationServices::Notifications
{
	using Clock = std::chrono::steady_clock;

	struct Toast
	{
		DMUI_ClientHandle owner{};
		DMUI_StatusSeverity severity{};
		std::string clientName;
		std::string title;
		std::string message;
		std::chrono::milliseconds duration{};
		uint64_t id{};
		uint32_t count{ 1 };
		Clock::duration remaining{};
		std::optional<Clock::time_point> lastUpdate;
	};

	class Queue
	{
	public:
		static constexpr size_t kVisibleLimit = 4;
		static constexpr size_t kPendingLimit = 32;

		// Returns true only when the oldest waiting toast was dropped.
		[[nodiscard]] bool Post(Toast a_toast)
		{
			auto* previous = !m_pending.empty() ? &m_pending.back() :
				!m_visible.empty() ? &m_visible.back() : nullptr;
			if (previous && previous->id == m_lastPosted &&
				previous->owner == a_toast.owner &&
				previous->severity == a_toast.severity &&
				previous->title == a_toast.title &&
				previous->message == a_toast.message &&
				previous->duration == a_toast.duration)
			{
				if (previous->count < (std::numeric_limits<uint32_t>::max)())
					++previous->count;
				previous->remaining = previous->duration;
				previous->lastUpdate.reset();
				return false;
			}
			a_toast.id = ++m_lastPosted;
			a_toast.remaining = a_toast.duration;
			m_pending.push_back(std::move(a_toast));
			if (m_pending.size() <= kPendingLimit)
				return false;
			m_pending.pop_front();
			return true;
		}

		void Advance(Clock::time_point a_now, uint64_t a_hovered = 0)
		{
			for (auto& toast : m_visible)
			{
				if (!toast.lastUpdate)
					continue;
				if (toast.id != a_hovered)
					toast.remaining -= a_now - *toast.lastUpdate;
				toast.lastUpdate = a_now;
			}
			std::erase_if(m_visible, [](const Toast& a_toast) {
				return a_toast.lastUpdate && a_toast.remaining <= Clock::duration::zero();
			});
			while (m_visible.size() < kVisibleLimit && !m_pending.empty())
			{
				m_visible.push_back(std::move(m_pending.front()));
				m_pending.pop_front();
			}
		}

		void Presented(uint64_t a_id, Clock::time_point a_now) noexcept
		{
			for (auto& toast : m_visible)
				if (toast.id == a_id && !toast.lastUpdate)
					toast.lastUpdate = a_now;
		}

		[[nodiscard]] bool Empty() const noexcept
		{
			return m_visible.empty() && m_pending.empty();
		}

		[[nodiscard]] const std::vector<Toast>& Visible() const noexcept { return m_visible; }
		[[nodiscard]] const std::deque<Toast>& Pending() const noexcept { return m_pending; }

	private:
		uint64_t m_lastPosted{};
		std::vector<Toast> m_visible;
		std::deque<Toast> m_pending;
	};

#if defined(DMUI_UI_TESTING)
	[[nodiscard]] Queue Snapshot();
#endif
}
