#include <RE/C/CursorMenu.h>
#include <RE/M/MENU_RENDER_CONTEXT.h>
#include <REX/REX.h>

#include "PlatformImGuiInternal.h"

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/ControllerNavigation.h>
#include <Support/Detours.h>

#include <atomic>

namespace DearModdingUI::PlatformImGuiDetail
{
	namespace
	{
		using namespace std::literals;
		using RenderCondition = bool (*)(
			const RE::CursorMenu*,
			RE::MENU_RENDER_CONTEXT,
			const RE::BSFixedString&);
		std::atomic<RenderCondition> s_previous{ nullptr };
		constexpr uint32_t kRenderConditionSlot{ 7 };

		// The host draws here while the cursor is open, above the console.
		bool RenderCursor(
			const RE::CursorMenu* a_menu,
			RE::MENU_RENDER_CONTEXT a_reason,
			const RE::BSFixedString& a_renderer) noexcept
		{
			const auto previous = s_previous.load(std::memory_order_acquire);
			if (!previous)
			{
				REX::ERROR("DearModdingUI: native cursor hook lost its predecessor"sv);
				return false;
			}
			const auto accepted = previous(a_menu, a_reason, a_renderer);
			if (a_reason != RE::MENU_RENDER_CONTEXT::kRenderScreenspace)
				return accepted;
			PlatformImGui::DrawUiPass(PlatformImGui::UiPassSite::kCursorMenu);
			return accepted &&
				!(DearModdingUI::CurrentInputMode() != DearModdingUI::HostInputMode::kGameplay &&
					DearModdingUI::ControllerNavigation::IsNavigating());
		}
	}

	bool InstallGameCursorHook() noexcept
	{
		const REL::Relocation<uintptr_t> vtable{ RE::CursorMenu::VTABLE[0] };
		auto** slots = reinterpret_cast<void**>(vtable.address());
		const auto current = reinterpret_cast<RenderCondition>(slots[kRenderConditionSlot]);
		if (!current || current == &RenderCursor)
		{
			REX::ERROR("DearModdingUI: native cursor render predecessor is unavailable"sv);
			return false;
		}
		s_previous.store(current, std::memory_order_release);
		const auto previous = Support::DetourVTable(
			vtable.address(), reinterpret_cast<uintptr_t>(&RenderCursor), kRenderConditionSlot);
		if (!previous)
		{
			REX::ERROR("DearModdingUI: native cursor render hook installation failed"sv);
			return false;
		}
		s_previous.store(reinterpret_cast<RenderCondition>(previous), std::memory_order_release);
		return true;
	}
}
