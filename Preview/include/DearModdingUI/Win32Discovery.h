#pragma once

#include <cstdint>

struct HINSTANCE__;

namespace dmui::detail
{
	using Win32Module = HINSTANCE__*;
	using Win32Procedure = intptr_t (__stdcall*)();

	extern "C" __declspec(dllimport) Win32Module __stdcall GetModuleHandleW(
		const wchar_t* a_moduleName);
	extern "C" __declspec(dllimport) Win32Procedure __stdcall GetProcAddress(
		Win32Module a_module,
		const char* a_symbol);

	[[nodiscard]] inline Win32Module HostModule() noexcept
	{
		static const auto module = GetModuleHandleW(nullptr);
		return module;
	}

	template <class Function>
	[[nodiscard]] Function ResolveHostSymbol(const char* a_symbol) noexcept
	{
		const auto module = HostModule();
		return module ?
			reinterpret_cast<Function>(GetProcAddress(module, a_symbol)) :
			nullptr;
	}
}
