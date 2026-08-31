#include <Support/Detours.h>

#include <Windows.h>

#include <cstring>

namespace Addictol::Support
{
	ScopeLock::ScopeLock(uintptr_t a_target, size_t a_size) noexcept :
		m_target(a_target),
		m_size(a_size)
	{
		m_unlocked = VirtualProtect(
			reinterpret_cast<void*>(m_target),
			m_size,
			PAGE_EXECUTE_READWRITE,
			reinterpret_cast<DWORD*>(&m_oldProtection)) != FALSE;
	}

	ScopeLock::ScopeLock(void* a_target, size_t a_size) noexcept :
		ScopeLock(reinterpret_cast<uintptr_t>(a_target), a_size)
	{}

	ScopeLock::~ScopeLock() noexcept
	{
		if (!m_unlocked)
			return;

		DWORD ignored{};
		(void)VirtualProtect(
			reinterpret_cast<void*>(m_target),
			m_size,
			m_oldProtection,
			&ignored);
		(void)FlushInstructionCache(
			GetCurrentProcess(),
			reinterpret_cast<void*>(m_target),
			m_size);
	}

	bool ScopeLock::HasUnlocked() const noexcept
	{
		return m_unlocked;
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
		const ScopeLock lock{ slot, sizeof(void*) };
		if (!lock.HasUnlocked())
			return 0;

		return reinterpret_cast<uintptr_t>(InterlockedExchangePointer(
			reinterpret_cast<void* volatile*>(slot),
			reinterpret_cast<void*>(a_function)));
	}

	uintptr_t DetourIAT(
		const char* a_importModule,
		const char* a_functionName,
		uintptr_t a_function) noexcept
	{
		if (!a_importModule || !a_functionName || !a_function)
			return 0;

		const auto module = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
		const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
		if (!dosHeader || dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
			return 0;

		const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
			module + static_cast<uintptr_t>(dosHeader->e_lfanew));
		if (ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
			ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
			return 0;

		const auto& imports = ntHeaders->OptionalHeader.DataDirectory[
			IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (!imports.VirtualAddress || !imports.Size)
			return 0;

		auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
			module + imports.VirtualAddress);
		for (; descriptor->Name; ++descriptor)
		{
			const auto* library = reinterpret_cast<const char*>(
				module + descriptor->Name);
			if (_stricmp(library, a_importModule) != 0)
				continue;
			if (!descriptor->OriginalFirstThunk || !descriptor->FirstThunk)
				return 0;

			auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(
				module + descriptor->OriginalFirstThunk);
			auto* functions = reinterpret_cast<IMAGE_THUNK_DATA64*>(
				module + descriptor->FirstThunk);
			for (; names->u1.Ordinal; ++names, ++functions)
			{
				if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))
					continue;

				const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
					module + names->u1.AddressOfData);
				if (std::strcmp(
						reinterpret_cast<const char*>(import->Name),
						a_functionName) != 0)
					continue;

				auto* slot = &functions->u1.Function;
				const ScopeLock lock{ slot, sizeof(*slot) };
				if (!lock.HasUnlocked())
					return 0;

				return static_cast<uintptr_t>(InterlockedExchange64(
					reinterpret_cast<volatile LONG64*>(slot),
					static_cast<LONG64>(a_function)));
			}
			return 0;
		}
		return 0;
	}
}
