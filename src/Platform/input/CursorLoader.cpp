#include <Platform/input/CursorLoader.h>
#include <DearModdingUI/VisualDecisions.h>

#include <Windows.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <REX/REX.h>

#undef ERROR

namespace DearModdingUI::CursorLoader
{
	namespace
	{
		HWND g_window{ nullptr };
		bool g_owned{ false };
		bool g_previousNoMouseCursorChange{ false };
		Source g_source{ Source::kSoftware };

		void RequestCursorUpdate() noexcept
		{
			if (!HasFocus())
				return;
			POINT position{};
			RECT client{};
			if (!GetCursorPos(&position) ||
				!ScreenToClient(g_window, &position) ||
				!GetClientRect(g_window, &client) ||
				!PtInRect(&client, position))
				return;

			// Let the window's current owner choose its cursor on the window thread.
			if (!PostMessageW(
					g_window,
					WM_SETCURSOR,
					reinterpret_cast<WPARAM>(g_window),
					MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)))
			{
				REX::WARN(
					"DearModdingUI: cursor update could not be queued (Windows error {})",
					GetLastError());
			}
		}
	}

	void Initialize(void* a_window, Source a_source) noexcept
	{
		Shutdown();
		if (!a_window || !ImGui::GetCurrentContext())
		{
			REX::ERROR("DearModdingUI: cursor binding requires a window and ImGui context");
			return;
		}
		g_window = static_cast<HWND>(a_window);
		g_source = a_source;
		auto& io = ImGui::GetIO();
		g_previousNoMouseCursorChange =
			(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) != 0;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	}

	bool HasFocus() noexcept
	{
		const auto focused = GetForegroundWindow();
		return g_window && focused &&
			(focused == g_window || IsChild(g_window, focused));
	}

	void PrepareFrame(
		bool a_modalVisible) noexcept
	{
		const auto cursor = DecideCursorPresentation(
			g_window && a_modalVisible && HasFocus());
		ImGui::GetIO().MouseDrawCursor =
			g_source == Source::kSoftware && cursor.drawSoftwareCursor;

		switch (DecideCursorTransition(g_owned, cursor.hideOperatingSystemCursor))
		{
		case CursorOwnershipTransition::kAcquire:
			g_owned = true;
			RequestCursorUpdate();
			break;
		case CursorOwnershipTransition::kRelease:
			g_owned = false;
			RequestCursorUpdate();
			break;
		default:
			break;
		}
	}

	void ApplyNativePosition(float a_x, float a_y) noexcept
	{
		auto& context = *ImGui::GetCurrentContext();
		for (auto index = context.InputEventsQueue.Size; index > 0; --index)
		{
			if (context.InputEventsQueue[index - 1].Type == ImGuiInputEventType_MousePos)
				context.InputEventsQueue.erase(context.InputEventsQueue.Data + index - 1);
		}
		// Native position is a frame snapshot; queued OS positions must not win or delay clicks.
		context.IO.MousePos = { a_x, a_y };
	}

	bool HandleWindowMessage(
		void* a_window,
		uint32_t a_message,
		uint64_t a_lparam) noexcept
	{
		if (a_window != g_window || !g_owned || !HasFocus() ||
			a_message != WM_SETCURSOR ||
			LOWORD(a_lparam) != HTCLIENT)
			return false;
		SetCursor(nullptr);
		return true;
	}

	void Shutdown() noexcept
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::GetIO().MouseDrawCursor = false;
			if (g_window && !g_previousNoMouseCursorChange)
			{
				ImGui::GetIO().ConfigFlags &=
					~ImGuiConfigFlags_NoMouseCursorChange;
			}
		}
		if (g_owned)
		{
			g_owned = false;
			RequestCursorUpdate();
		}
		g_window = nullptr;
		g_previousNoMouseCursorChange = false;
		g_source = Source::kSoftware;
	}
}
