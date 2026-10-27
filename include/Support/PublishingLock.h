#pragma once

#include <mutex>
#include <type_traits>
#include <utility>

namespace DearModdingUI::Support
{
	// Holds an owner's lock and republishes its lock-free mirrors before unlocking.
	template <class Publish>
	class PublishingLock
	{
	public:
		static_assert(std::is_nothrow_invocable_v<Publish&>);

		PublishingLock(std::mutex& a_mutex, Publish a_publish) :
			m_lock(a_mutex),
			m_publish(std::move(a_publish))
		{}

		~PublishingLock() noexcept
		{
			m_publish();
		}

		PublishingLock(const PublishingLock&) = delete;
		PublishingLock& operator=(const PublishingLock&) = delete;

	private:
		std::scoped_lock<std::mutex> m_lock;
		Publish m_publish;
	};
}
