#pragma once

#include <cstddef>
#include <cstdint>

namespace Addictol::Support
{
	class ScopeLock
	{
	public:
		ScopeLock(uintptr_t a_target, size_t a_size) noexcept;
		ScopeLock(void* a_target, size_t a_size) noexcept;
		~ScopeLock() noexcept;

		ScopeLock(const ScopeLock&) = delete;
		ScopeLock(ScopeLock&&) = delete;
		ScopeLock& operator=(const ScopeLock&) = delete;
		ScopeLock& operator=(ScopeLock&&) = delete;

		[[nodiscard]] bool HasUnlocked() const noexcept;

	private:
		bool m_unlocked{ false };
		uint32_t m_oldProtection{ 0 };
		uintptr_t m_target{ 0 };
		size_t m_size{ 0 };
	};

	[[nodiscard]] uintptr_t DetourVTable(
		uintptr_t a_target,
		uintptr_t a_function,
		uint32_t a_index) noexcept;
	[[nodiscard]] uintptr_t DetourIAT(
		const char* a_importModule,
		const char* a_functionName,
		uintptr_t a_function) noexcept;
}
