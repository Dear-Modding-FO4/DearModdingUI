#include "../Harness.h"
#include "../support/ImGuiTestContext.h"

#include <Platform/input/CursorLoader.h>

#include <Windows.h>

namespace vmm_tests
{
	namespace
	{
		class CursorWindow
		{
		public:
			CursorWindow() :
				window(CreateWindowExW(
					0, L"STATIC", L"DMUI cursor ownership test", WS_POPUP,
					0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr))
			{
				require(window != nullptr, "hidden cursor test window creation failed");
			}

			~CursorWindow()
			{
				DearModdingUI::CursorLoader::Shutdown();
				DestroyWindow(window);
			}

			CursorWindow(const CursorWindow&) = delete;
			CursorWindow& operator=(const CursorWindow&) = delete;

			HWND window;
		};
	}

	void run_cursor_ownership_checks(Runner& runner)
	{
		namespace Cursor = DearModdingUI::CursorLoader;

		runner.test("unfocused and detached cursor bindings leave the OS cursor alone", [] {
			const support::ImGuiTestContext context;
			const CursorWindow window;
			const auto osCursor = GetCursor();
			auto& io = ImGui::GetIO();
			Cursor::Initialize(window.window);
			require(!Cursor::HasFocus(), "a hidden test window unexpectedly has focus");
			io.MouseDrawCursor = true;
			Cursor::PrepareFrame(true);
			require(!io.MouseDrawCursor,
				"a visible modal request drew a cursor while its window was unfocused");
			const auto cursorMessage = static_cast<uint64_t>(
				MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
			require(!Cursor::HandleWindowMessage(
						window.window, WM_SETCURSOR, cursorMessage),
				"an unfocused window's cursor message was consumed");

			Cursor::Shutdown();
			Cursor::PrepareFrame(true);
			require(!io.MouseDrawCursor && !Cursor::HasFocus() &&
					!Cursor::HandleWindowMessage(
						window.window, WM_SETCURSOR, cursorMessage),
				"a detached cursor acquired input or cursor ownership");
			require(GetCursor() == osCursor,
				"unfocused cursor lifecycle changed the OS cursor");
		});
	}
}
