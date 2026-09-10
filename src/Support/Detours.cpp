#include <Support/Detours.h>

#include <Windows.h>

#include <cstddef>

namespace Addictol::Support
{
	namespace
	{
		class MemoryProtectionScope
		{
		public:
			MemoryProtectionScope(void* a_target, size_t a_size) noexcept :
				m_target(a_target),
				m_size(a_size),
				m_writable(VirtualProtect(
					a_target, a_size, PAGE_EXECUTE_READWRITE, &m_oldProtection) != FALSE)
			{}

			~MemoryProtectionScope() noexcept
			{
				if (!m_writable)
					return;

				DWORD ignored{};
				(void)VirtualProtect(m_target, m_size, m_oldProtection, &ignored);
				(void)FlushInstructionCache(GetCurrentProcess(), m_target, m_size);
			}

			MemoryProtectionScope(const MemoryProtectionScope&) = delete;
			MemoryProtectionScope& operator=(const MemoryProtectionScope&) = delete;

			[[nodiscard]] bool IsWritable() const noexcept { return m_writable; }

		private:
			void* m_target;
			size_t m_size;
			DWORD m_oldProtection{};
			bool m_writable;
		};
	}

	uintptr_t DetourVTable(
		uintptr_t a_target,
		uintptr_t a_function,
		uint32_t a_index) noexcept
	{
		if (!a_target || !a_function)
			return 0;

		auto** slot = reinterpret_cast<void**>(
			a_target + static_cast<uintptr_t>(a_index) * sizeof(void*));
		const MemoryProtectionScope protection{ slot, sizeof(void*) };
		if (!protection.IsWritable())
			return 0;

		return reinterpret_cast<uintptr_t>(InterlockedExchangePointer(
			reinterpret_cast<void* volatile*>(slot),
			reinterpret_cast<void*>(a_function)));
	}
}
