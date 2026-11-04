#include "PlatformImGuiInternal.h"

#include <Platform/menus/HostMenu.h>
#include <Platform/input/CursorLoader.h>
#include <Platform/input/EngineFocus.h>
#include <DearModdingUI/host/HeldInput.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/PointerRouting.h>
#include <DearModdingUI/host/ControllerNavigation.h>
#include <Platform/input/GameInput.h>

#include <imgui/imgui.h>

namespace DearModdingUI::PlatformImGuiDetail
{
	using namespace PlatformImGui;

	void SetModalInputStateLocked(bool a_visible) noexcept
	{
		auto& context = Context();
		const auto active = a_visible && DearModdingUI::EngineFocus::Active();
		const auto previous = context.drawingEnabled.exchange(
			active,
			std::memory_order_acq_rel);
		GameInput::SuppressGameInput(active);
		if (previous != active)
			ResetGameCursorWaitLocked();
		if (previous == active || !ImGui::GetCurrentContext())
			return;

		auto& io = ImGui::GetIO();
		if (!active)
			DearModdingUI::ControllerNavigation::Reset();
		io.ClearInputKeys();
		io.ClearInputMouse();
		ClearTextInput();
		DearModdingUI::PointerRouting::Reset();
	}

	void ApplyDrawingRequestLocked(bool a_enabled) noexcept
	{
		SetModalInputStateLocked(a_enabled);
		const auto active = Context().drawingEnabled.load(std::memory_order_acquire);
		DearModdingUI::HostMenu::Handle(
			active ?
				DearModdingUI::HostMenu::Event::kOpen :
				DearModdingUI::HostMenu::Event::kClose);
		if (ImGui::GetCurrentContext())
			DearModdingUI::CursorLoader::PrepareFrame(active);
	}

	void CloseModalStateLocked(
		DearModdingUI::HostMenu::Event a_event) noexcept
	{
		(void)DearModdingUI::ReleaseHeldInput();
		SetModalInputStateLocked(false);
		DearModdingUI::EndOverlayFocus(
			a_event == DearModdingUI::HostMenu::Event::kShutdown ?
				DMUI_OVERLAY_FOCUS_END_HOST_UNAVAILABLE :
				DMUI_OVERLAY_FOCUS_END_INTERRUPTED);
		DearModdingUI::CloseMenu();
		DearModdingUI::HostMenu::Handle(a_event);
		if (ImGui::GetCurrentContext())
			DearModdingUI::CursorLoader::PrepareFrame(false);
	}

}
