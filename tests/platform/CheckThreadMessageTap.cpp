#include <Windows.h>

#include "../Harness.h"
#include <Platform/input/ThreadMessageTap.h>

#include <string>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI;

		struct Observed
		{
			std::vector<std::pair<char16_t, bool>> text;
			bool claimKeys{ false };
			bool swallow{ false };
			uint32_t cursorHit{ 0 };
			void* cursorWindow{ nullptr };
			// What the window procedure itself received.
			std::wstring delivered;
		} g_observed;

		// A subclass that forwards nothing, like a wrapper that eats its input.
		LRESULT CALLBACK EatingProc(HWND a_window, UINT a_message, WPARAM a_wparam, LPARAM a_lparam)
		{
			if (a_message == WM_CHAR)
			{
				g_observed.delivered.push_back(static_cast<wchar_t>(a_wparam));
				return 0;
			}
			return a_message == WM_SETCURSOR ? 0 : DefWindowProcW(a_window, a_message, a_wparam, a_lparam);
		}

		void Pump()
		{
			MSG message;
			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
				DispatchMessageW(&message);
		}

		struct HiddenWindow
		{
			HiddenWindow()
			{
				WNDCLASSW windowClass{};
				windowClass.lpfnWndProc = &EatingProc;
				windowClass.hInstance = GetModuleHandleW(nullptr);
				windowClass.lpszClassName = L"dmui-tap-test";
				RegisterClassW(&windowClass);
				handle = CreateWindowExW(
					0, windowClass.lpszClassName, L"", WS_POPUP, 0, 0, 8, 8,
					nullptr, nullptr, windowClass.hInstance, nullptr);
			}

			~HiddenWindow()
			{
				ThreadMessageTap::Uninstall();
				if (handle)
					DestroyWindow(handle);
			}

			HWND handle{ nullptr };
		};

		ThreadMessageTap::Sink MakeSink()
		{
			return {
				[](uint32_t) noexcept { return g_observed.claimKeys; },
				[](char16_t a_unit, bool a_claimed) noexcept {
					g_observed.text.emplace_back(a_unit, a_claimed);
					return g_observed.swallow;
				},
				nullptr,
				[](void* a_window, uint32_t a_hit) noexcept {
					g_observed.cursorWindow = a_window;
					g_observed.cursorHit = a_hit;
				}
			};
		}
	}

	void run_thread_message_tap_checks(Runner& runner)
	{
		runner.test("Thread tap sees chars a non-forwarding window procedure would eat", [] {
			g_observed = {};
			HiddenWindow window;
			require(window.handle != nullptr, "test window could not be created");
			require(ThreadMessageTap::Install(MakeSink()), "tap did not install");

			PostMessageW(window.handle, WM_CHAR, L'q', 0);
			Pump();
			require(g_observed.text.size() == 1 && g_observed.text[0].first == u'q',
				"the tap missed a posted character");
			require(g_observed.delivered == L"q", "an unswallowed character did not reach the window");

			// A swallowed char becomes WM_NULL before the window sees it.
			g_observed.swallow = true;
			PostMessageW(window.handle, WM_CHAR, L'w', 0);
			Pump();
			require(g_observed.text.size() == 2 && g_observed.delivered == L"q",
				"a swallowed character still reached the window");

			// The char of a hotkey-claimed key press is reported as claimed.
			g_observed.claimKeys = true;
			PostMessageW(window.handle, WM_KEYDOWN, 'E', 0);
			PostMessageW(window.handle, WM_CHAR, L'e', 0);
			PostMessageW(window.handle, WM_CHAR, L'r', 0);
			Pump();
			require(g_observed.text.size() == 4 && g_observed.text[2].second && !g_observed.text[3].second,
				"a claim did not cover exactly the key press's character");
		});

		runner.test("Thread tap reports cursor requests after the window procedure runs", [] {
			g_observed = {};
			HiddenWindow window;
			require(ThreadMessageTap::Install(MakeSink()), "tap did not install");

			SendMessageW(window.handle, WM_SETCURSOR, reinterpret_cast<WPARAM>(window.handle),
				MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
			require(g_observed.cursorWindow == window.handle && g_observed.cursorHit == HTCLIENT,
				"the tap missed a sent cursor request");
		});

		runner.test("Thread tap binds to the installing thread and rejects a second install", [] {
			HiddenWindow window;
			require(ThreadMessageTap::Install(MakeSink()), "tap did not install");
			require(ThreadMessageTap::Thread() == GetCurrentThreadId(), "tap bound to another thread");
			require(!ThreadMessageTap::Install(MakeSink()),
				"a second install leaked a hook pair");
		});
	}
}
