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

		runner.test("native cursor snapshots keep queued clicks and scrolling on the displayed position", [] {
			support::ImGuiTestContext context;
			auto& io = ImGui::GetIO();
			float wheelObserved{};
			const auto draw = [&] {
				context.BeginWindow(
					"##NativeCursorInput", { 0, 0 }, { 640, 480 },
					ImGuiWindowFlags_NoDecoration, ImGuiCond_Always);
				ImGui::SetCursorScreenPos({ 120, 100 });
				const auto pressed = ImGui::Button("Native target", { 160, 40 });
				wheelObserved = io.MouseWheel;
				context.EndWindow(true);
				return pressed;
			};
			(void)draw();
			(void)draw();
			io.AddMousePosEvent(20, 20);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			Cursor::ApplyNativePosition(160, 120);
			require(!draw() && io.MouseDown[ImGuiMouseButton_Left] &&
					io.MouseClickedPos[ImGuiMouseButton_Left].x == 160 &&
					io.MouseClickedPos[ImGuiMouseButton_Left].y == 120,
				"click was delayed or hit the stale OS cursor position");
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			io.AddMousePosEvent(20, 20);
			Cursor::ApplyNativePosition(160, 120);
			require(draw(), "native cursor click did not activate its visible target");
			io.AddMouseWheelEvent(0, 1);
			io.AddMousePosEvent(20, 20);
			Cursor::ApplyNativePosition(165, 125);
			(void)draw();
			require(wheelObserved == 1 && io.MousePos.x == 165 && io.MousePos.y == 125,
				"continuous native movement starved wheel events or reverted to the OS cursor");
		});

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
