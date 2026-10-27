#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

namespace DearModdingUI::Support
{
	// Owners advance after a change is visible; readers compare it lock-free.
	class Revision
	{
	public:
		void Advance() noexcept
		{
			m_value.fetch_add(1, std::memory_order_acq_rel);
		}

		[[nodiscard]] uint64_t Current() const noexcept
		{
			return m_value.load(std::memory_order_acquire);
		}

	private:
		std::atomic<uint64_t> m_value{ 1 };
	};

	// One-thread cache; Value& builders rebuild in place so views stay valid.
	template <class Key, class Value>
	class Derived
	{
	public:
		// Read a probe key before the inputs; a racing change only rebuilds again.
		template <class Probe>
		[[nodiscard]] bool Current(const Probe& a_key) const noexcept
		{
			return m_key && *m_key == a_key;
		}

		template <class Probe, class Build>
		const Value& Get(const Probe& a_key, Build&& a_build)
		{
			if (!Current(a_key))
			{
				if constexpr (std::is_invocable_v<Build&, Value&>)
					a_build(m_value);
				else
					m_value = a_build();
				m_key.emplace(a_key);
			}
			return m_value;
		}

		[[nodiscard]] const Key* CachedKey() const noexcept
		{
			return m_key ? &*m_key : nullptr;
		}

		void Invalidate() noexcept
		{
			m_key.reset();
		}

	private:
		std::optional<Key> m_key;
		Value m_value{};
	};
}
