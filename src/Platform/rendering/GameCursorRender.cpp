#include <RE/B/BSGraphics.h>
#include <RE/C/CursorMenu.h>
#include <RE/M/MenuCursor.h>
#include <RE/M/MENU_RENDER_CONTEXT.h>
#include <RE/U/UI.h>
#include <REX/REX.h>

#include "PlatformImGuiInternal.h"

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <Platform/input/CursorLoader.h>
#include <Support/Detours.h>

#include <atomic>

namespace Addictol::platformImguiDetail
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
			if (a_reason != RE::MENU_RENDER_CONTEXT::kRenderScreenspace ||
				!DearModdingUI::IsMenuVisible())
				return accepted;

			DearModdingUI::RenderExecution::Guard execution{
				DearModdingUI::RenderExecution::Phase::kFrameDraw
			};
			const ContextLock lock;
			auto& context = Context();
			if (context.backend.load(std::memory_order_acquire) != Backend::kReady ||
				context.attachmentLifecycle != ImguiPlatform::AttachmentLifecycle::kActive ||
				!DearModdingUI::CursorLoader::HasFocus())
				return accepted;

			ApplyDrawingRequestLocked(DearModdingUI::IsMenuVisible());
			if (!context.drawingEnabled.load(std::memory_order_acquire))
				return accepted;
			if (!DearModdingUI::CarrierMenu::IsOpen())
			{
				NoteGameCursorUnavailableLocked("the cursor carrier has not joined the menu stack");
				return accepted;
			}
			const auto* ui = RE::UI::GetSingleton();
			if (!accepted || !a_menu->OnStack() ||
				!a_menu->IsMenuDisplayEnabled() ||
				!a_menu->hasDoneFirstAdvanceMovie ||
				a_menu->menuFlags.any(RE::UI_MENU_FLAGS::kCustomRendering) ||
				!a_menu->uiMovie || !a_menu->uiMovie->GetVisible() ||
				!a_menu->cursor || !ui || !ui->menuSystemVisible)
			{
				NoteGameCursorUnavailableLocked("the game's cursor is not drawable in screen space");
				return accepted;
			}
			const auto* renderer = RE::BSGraphics::GetRendererData();
			const auto* window = RE::BSGraphics::GetCurrentRendererWindow();
			if (!renderer || !renderer->initialized || !window ||
				reinterpret_cast<ID3D11Device*>(renderer->device) != context.attachment.device.Get() ||
				reinterpret_cast<ID3D11DeviceContext*>(renderer->context) != context.attachment.context.Get() ||
				reinterpret_cast<HWND>(window->hwnd) != context.attachment.window)
			{
				NoteGameCursorUnavailableLocked("the native UI renderer no longer matches the active attachment");
				return accepted;
			}
			const auto* cursor = RE::MenuCursor::GetSingleton();
			if (!cursor || !cursor->registeredCursors)
			{
				NoteGameCursorUnavailableLocked("the native cursor has no registered position source");
				return accepted;
			}
			(void)execution.NoteBinding(context.attachmentGeneration);
			DrawBeforeGameCursorLocked({
				static_cast<float>(cursor->cursorPosX),
				static_cast<float>(cursor->cursorPosY)
			});
			return accepted;
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
		REX::INFO("DearModdingUI: native cursor screen-space render hook installed"sv);
		return true;
	}
}
