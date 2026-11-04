#include <Platform/input/CursorLoader.h>
#include <Platform/input/EngineFocus.h>
#include <DearModdingUI/VisualDecisions.h>
#include <DearModdingUI/host/ControllerNavigation.h>

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

		// SetCursor is thread-affine; later WM_SETCURSORs reach the tap.
		void HideOsCursorNow() noexcept
		{
			if (EngineFocus::Active() && EngineFocus::OnUiThread())
				SetCursor(nullptr);
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

	void PrepareFrame(
		bool a_modalVisible) noexcept
	{
		const auto cursor = DecideCursorPresentation(
			g_window && a_modalVisible && EngineFocus::Active());
		ImGui::GetIO().MouseDrawCursor =
			g_source == Source::kSoftware && cursor.drawSoftwareCursor &&
			!ControllerNavigation::IsNavigating();

		switch (DecideCursorTransition(g_owned, cursor.hideOperatingSystemCursor))
		{
		case CursorOwnershipTransition::kAcquire:
			g_owned = true;
			HideOsCursorNow();
			break;
		case CursorOwnershipTransition::kRelease:
			g_owned = false;
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

	bool ApplySetCursor(void* a_window, uint32_t a_hitTest) noexcept
	{
		if (a_window != g_window || !g_owned || !EngineFocus::Active() || a_hitTest != HTCLIENT)
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
		g_owned = false;
		g_window = nullptr;
		g_previousNoMouseCursorChange = false;
		g_source = Source::kSoftware;
	}
}
