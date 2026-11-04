#include <Platform/input/ThreadMessageTap.h>

#include <Windows.h>

#include <atomic>
#include <utility>

namespace DearModdingUI::ThreadMessageTap
{
	namespace
	{
		// Written at install, read only on the tapped thread afterwards.
		Sink s_sink{};
		HHOOK s_getMessage{ nullptr };
		HHOOK s_callWndProcRet{ nullptr };
		DWORD s_thread{ 0 };
		std::atomic<UINT> s_codePage{ CP_ACP };
		// Pairs a char with the key press that produced it.
		bool s_claimNextChar{ false };

		void RefreshCodePage(DWORD a_thread) noexcept
		{
			const auto layout = reinterpret_cast<uintptr_t>(GetKeyboardLayout(a_thread));
			UINT codePage{ 0 };
			if (!GetLocaleInfoW(
					LOWORD(layout),
					LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
					reinterpret_cast<LPWSTR>(&codePage),
					sizeof(codePage) / sizeof(wchar_t)) ||
				!codePage)
				codePage = CP_ACP;
			s_codePage.store(codePage, std::memory_order_release);
		}

		// An ANSI window posts bytes in the input language's code page.
		[[nodiscard]] char16_t DecodeAnsi(WPARAM a_unit) noexcept
		{
			const char bytes[2]{
				static_cast<char>(a_unit > 0xFF ? HIBYTE(a_unit) : LOBYTE(a_unit)),
				static_cast<char>(LOBYTE(a_unit))
			};
			wchar_t wide{ 0 };
			MultiByteToWideChar(
				s_codePage.load(std::memory_order_acquire),
				MB_PRECOMPOSED,
				bytes,
				a_unit > 0xFF ? 2 : 1,
				&wide,
				1);
			return wide;
		}

		void TapChar(MSG& a_message) noexcept
		{
			const auto claimed = std::exchange(s_claimNextChar, false);
			if (!s_sink.text)
				return;
			const auto unit = IsWindowUnicode(a_message.hwnd) ?
				static_cast<char16_t>(a_message.wParam < 0x10000 ? a_message.wParam : 0) :
				DecodeAnsi(a_message.wParam);
			if (unit && s_sink.text(unit, claimed))
				a_message.message = WM_NULL;
		}

		LRESULT CALLBACK GetMessageProc(int a_code, WPARAM a_wparam, LPARAM a_lparam) noexcept
		{
			// A peek would be seen again when the message is removed.
			if (a_code == HC_ACTION && a_wparam == PM_REMOVE && a_lparam)
			{
				auto& message = *reinterpret_cast<MSG*>(a_lparam);
				switch (message.message)
				{
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
					s_claimNextChar = s_sink.keyDown &&
						s_sink.keyDown(static_cast<uint32_t>(message.wParam));
					break;
				case WM_CHAR:
				case WM_SYSCHAR:
					TapChar(message);
					break;
				default:
					break;
				}
			}
			return CallNextHookEx(nullptr, a_code, a_wparam, a_lparam);
		}

		LRESULT CALLBACK CallWndProcRet(int a_code, WPARAM a_wparam, LPARAM a_lparam) noexcept
		{
			if (a_code == HC_ACTION && a_lparam)
			{
				const auto& message = *reinterpret_cast<const CWPRETSTRUCT*>(a_lparam);
				if (message.message == WM_SETCURSOR && s_sink.setCursor)
					s_sink.setCursor(message.hwnd, LOWORD(message.lParam));
				else if (message.message == WM_INPUTLANGCHANGE)
				{
					RefreshCodePage(0);
					if (s_sink.inputLanguageChanged)
						s_sink.inputLanguageChanged();
				}
			}
			return CallNextHookEx(nullptr, a_code, a_wparam, a_lparam);
		}
	}

	bool Install(const Sink& a_sink) noexcept
	{
		if (s_getMessage)
			return false;
		const auto thread = GetCurrentThreadId();

		// A thread hook needs the module that holds its procedure.
		HMODULE module{ nullptr };
		if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
					GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&GetMessageProc),
				&module))
			return false;

		s_sink = a_sink;
		s_claimNextChar = false;
		s_getMessage = SetWindowsHookExW(WH_GETMESSAGE, &GetMessageProc, module, thread);
		s_callWndProcRet = SetWindowsHookExW(WH_CALLWNDPROCRET, &CallWndProcRet, module, thread);
		if (s_getMessage && s_callWndProcRet)
		{
			RefreshCodePage(thread);
			s_thread = thread;
			return true;
		}
		Uninstall();
		return false;
	}

	void Uninstall() noexcept
	{
		if (s_getMessage)
			UnhookWindowsHookEx(s_getMessage);
		if (s_callWndProcRet)
			UnhookWindowsHookEx(s_callWndProcRet);
		s_getMessage = nullptr;
		s_callWndProcRet = nullptr;
		s_thread = 0;
		s_sink = {};
	}

	bool Installed() noexcept
	{
		return s_getMessage != nullptr;
	}

	uint32_t Thread() noexcept
	{
		return s_thread;
	}
}
