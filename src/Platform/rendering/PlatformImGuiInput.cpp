// CommonLib declarations must precede the Windows SDK macros.
#include <RE/U/UI.h>
#include <REX/REX.h>

#include "PlatformImGuiInternal.h"

#include <Platform/input/CursorLoader.h>
#include <Platform/input/EngineFocus.h>
#include <Platform/input/EngineKeys.h>
#include <DearModdingUI/host/ControllerNavigation.h>
#include <DearModdingUI/host/HeldInput.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/host/PointerRouting.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <Support/OnceFlag.h>

#include <imgui/imgui.h>

#include <array>
#include <atomic>

namespace DearModdingUI::PlatformImGuiDetail
{
	using namespace std::literals;
	using namespace PlatformImGui;

	static_assert(
		DMUI_POINTER_BUTTON_LEFT == 1u &&
		DMUI_POINTER_BUTTON_RIGHT == 2u &&
		DMUI_POINTER_BUTTON_MIDDLE == 4u &&
		DMUI_POINTER_BUTTON_X1 == 8u &&
		DMUI_POINTER_BUTTON_X2 == 16u);

	namespace
	{
		using EngineKeys::Edge;

		// Loading polls from job threads; the host is not thread-safe.
		void RequestToggle() noexcept
		{
			auto& context = Context();
			if (!context.callbacks.toggle)
				return;
			if (EngineFocus::OnUiThread())
				context.callbacks.toggle();
			else
				context.pendingToggles.fetch_add(1, std::memory_order_acq_rel);
		}

		// True if any key took the edge; a toggle runs once per press.
		[[nodiscard]] bool HandleHotkeys(
			const EngineKeys::HotkeyCodes& a_codes,
			uint32_t a_modifiers,
			bool a_pressed,
			bool a_repeat) noexcept
		{
			RefreshHotkeyContext();
			bool consumed{ false };
			bool toggle{ false };
			bool dropped{ false };
			for (const auto code : a_codes)
			{
				const auto result = DearModdingUI::Hotkeys::HandleKey(code, a_modifiers, a_pressed, a_repeat);
				consumed |= result != DearModdingUI::HotkeyMessageResult::kPassThrough;
				toggle |= result == DearModdingUI::HotkeyMessageResult::kMenuToggle;
				dropped |= result == DearModdingUI::HotkeyMessageResult::kConsumedPairDropped;
			}
			if (a_pressed && !a_repeat && !consumed)
			{
				REX::DEBUG(
					"DearModdingUI: no hotkey took code 0x{:02X} (modifiers 0x{:X})"sv,
					a_codes.count ? a_codes.values[0] : 0u,
					a_modifiers);
			}
			if (dropped)
				REX::WARN(
					"DearModdingUI: hotkey event queue overflowed; one press/release pair was dropped"sv);
			if (toggle)
				RequestToggle();
			return consumed;
		}

		[[nodiscard]] bool FeedsImGuiLocked() noexcept
		{
			return ImGui::GetCurrentContext() &&
				Context().backend.load(std::memory_order_acquire) == Backend::kReady;
		}

		// A release always lands so a closing menu cannot strand a key.
		void FeedKey(ImGuiKey a_key, bool a_down) noexcept
		{
			if (a_key == ImGuiKey_None)
				return;
			const ContextLock lock;
			if (FeedsImGuiLocked() &&
				(!a_down || Context().drawingEnabled.load(std::memory_order_acquire)))
				ImGui::GetIO().AddKeyEvent(a_key, a_down);
		}

		// ImGui shortcuts read the combined modifiers, not left/right keys.
		void FeedModifiers(uint32_t a_modifiers) noexcept
		{
			const ContextLock lock;
			if (!FeedsImGuiLocked())
				return;
			auto& io = ImGui::GetIO();
			io.AddKeyEvent(ImGuiMod_Ctrl, (a_modifiers & kHotkeyModifierControl) != 0);
			io.AddKeyEvent(ImGuiMod_Shift, (a_modifiers & kHotkeyModifierShift) != 0);
			io.AddKeyEvent(ImGuiMod_Alt, (a_modifiers & kHotkeyModifierAlt) != 0);
		}

		[[nodiscard]] bool ObserveKeyboard(const ButtonEdge& a_edge) noexcept
		{
			const auto pressed = a_edge.edge != Edge::kRelease;
			const auto repeat = a_edge.edge == Edge::kHold;
			// The polling thread may not own the keyboard layout.
			void* const layout = GetKeyboardLayout(EngineFocus::UiThread());
			const auto codes = EngineKeys::ToHotkeyCode(a_edge.id, layout);

			bool escapeConsumed{ false };
			if (a_edge.id == kEscapeVirtualKey)
			{
				const auto decision = DecideEscapeEdge(
					pressed, repeat,
					CurrentInputMode() != HostInputMode::kGameplay,
					Hotkeys::OwnsKey(kEscapeKeyCode));
				if (decision == EscapeDecision::kCapture)
				{
					Hotkeys::Claim(kEscapeKeyCode);
					const ContextLock lock;
					CaptureMenuEscapePress(
						CurrentInputMode(),
						PresentationServices::HasActiveDialog(),
						PresentationServices::ActiveDialogPopupId());
				}
				else if (decision == EscapeDecision::kReleaseAndForward)
					(void)Hotkeys::HandleKey(kEscapeKeyCode, 0, false, false);
				escapeConsumed = decision != EscapeDecision::kForward &&
					decision != EscapeDecision::kReleaseAndForward;
			}

			// A consumed Escape release still ends its claim.
			if ((!pressed || !escapeConsumed) &&
				HandleHotkeys(codes, a_edge.modifiers, pressed, repeat) && !escapeConsumed)
				return !repeat;

			// Held keys repeat inside ImGui, so only edges are fed.
			if (!repeat)
			{
				FeedModifiers(a_edge.modifiers);
				FeedKey(EngineKeys::ToImGuiKey(a_edge.id, layout), pressed);
			}
			return escapeConsumed && !repeat;
		}

		[[nodiscard]] bool ObserveGamepad(const ButtonEdge& a_edge) noexcept
		{
			const auto pressed = a_edge.edge != Edge::kRelease;
			HotkeyMessageResult result;
			{
				const ContextLock lock;
				result = ControllerNavigation::RouteButton(
					a_edge.id, pressed, a_edge.edge == Edge::kHold,
					a_edge.id >= KeyCatalog::kPadLT ?
						ControllerNavigation::AnalogValue(a_edge.value) : a_edge.value,
					CurrentHotkeyContextState());
			}
			// Toggle callbacks reacquire the platform lock.
			if (result == HotkeyMessageResult::kMenuToggle)
				RequestToggle();
			// Dismissal claims B without a hotkey result.
			return a_edge.edge != Edge::kHold &&
				(result != HotkeyMessageResult::kPassThrough || Hotkeys::OwnsKey(a_edge.id));
		}

		// Presses reach the page or ImGui; releases always settle held buttons.
		void FeedPointer(const ButtonEdge& a_edge, bool a_hotkeyConsumed) noexcept
		{
			using namespace DearModdingUI::PointerRouting;
			const auto wheel = IsMouseWheelCode(a_edge.id);
			const auto button = a_edge.id - static_cast<uint32_t>(DearModdingUI::KeyCatalog::kMouseButtonOffset);
			// Engine buttons beyond ImGui's range have no pointer meaning.
			if (!wheel && button >= static_cast<uint32_t>(ImGuiMouseButton_COUNT))
				return;
			const auto released = a_edge.edge == Edge::kRelease;

			const ContextLock lock;
			if (!FeedsImGuiLocked() ||
				(!released && !Context().drawingEnabled.load(std::memory_order_acquire)))
				return;
			const auto owner = DearModdingUI::PointerOwnerPage();
			const auto excluded = owner != DMUI_INVALID_PAGE_HANDLE && a_hotkeyConsumed;
			// The queue has no position, so the press samples the cursor.
			const auto position = SamplePointerLocked();
			auto& io = ImGui::GetIO();
			if (wheel)
			{
				const auto notches = a_edge.id == static_cast<uint32_t>(DearModdingUI::KeyCatalog::kMouseWheelOffset) ?
					1.0f : -1.0f;
				if (RouteWheel(owner, { 0.0f, notches }, { position.x, position.y }, excluded) ==
					Verdict::kForward)
					io.AddMouseWheelEvent(0.0f, notches);
				return;
			}
			const auto mask = 1u << button;
			// ImGui would drop focus and active widgets on a world click.
			if (released)
			{
				if (RouteRelease(owner, mask) == Verdict::kForward)
					io.AddMouseButtonEvent(static_cast<int>(button), false);
				return;
			}
			if (RoutePress(owner, mask, { position.x, position.y }, excluded) == Verdict::kForward)
			{
				io.AddMousePosEvent(position.x, position.y);
				io.AddMouseButtonEvent(static_cast<int>(button), true);
			}
		}

		[[nodiscard]] bool ObserveMouse(const ButtonEdge& a_edge) noexcept
		{
			const auto wheel = IsMouseWheelCode(a_edge.id);
			const auto pressed = a_edge.edge != Edge::kRelease;
			const auto repeat = a_edge.edge == Edge::kHold && !wheel;
			const EngineKeys::HotkeyCodes codes{ { a_edge.id }, 1 };
			auto consumed = HandleHotkeys(codes, a_edge.modifiers, pressed, repeat);
			// A wheel notch has no release, so it presses and releases.
			if (wheel && pressed)
				consumed = HandleHotkeys(codes, a_edge.modifiers, false, false) || consumed;
			if (repeat)
				return false;
			FeedPointer(a_edge, consumed);
			return consumed;
		}
	}

	DearModdingUI::HotkeyContextState CurrentHotkeyContextState() noexcept
	{
		const auto* ui = RE::UI::GetSingleton();
		return DearModdingUI::CurrentHotkeyContext(ui && ui->menuMode == 0);
	}

	void RefreshHotkeyContext() noexcept
	{
		DearModdingUI::Hotkeys::SetContext(CurrentHotkeyContextState());
	}

	void DrainQueuedToggles() noexcept
	{
		auto& context = Context();
		for (auto count = context.pendingToggles.exchange(0, std::memory_order_acq_rel); count > 0; --count)
		{
			if (context.callbacks.toggle)
				context.callbacks.toggle();
		}
	}
}

namespace DearModdingUI::PlatformImGui
{
	void LogEngineFocus(std::string_view a_context) noexcept
	{
		using namespace std::literals;
		const auto state = DearModdingUI::EngineFocus::Read();
		REX::INFO(
			"DearModdingUI: engine focus ({}): polling={} uiThread={} foregroundThread={}"sv,
			a_context,
			state.pollingEnabled,
			state.uiThread,
			DearModdingUI::EngineFocus::ForegroundThread());
	}

	bool ObserveButton(const ButtonEdge& a_edge) noexcept
	{
		// A press outside engine focus belongs to another window.
		if (a_edge.edge != EngineKeys::Edge::kRelease && !DearModdingUI::EngineFocus::Active())
		{
			static Support::OnceFlag s_logged;
			if (s_logged.First())
				LogEngineFocus("first press dropped");
			return false;
		}
		switch (a_edge.device)
		{
		case InputDevice::kKeyboard:
			return PlatformImGuiDetail::ObserveKeyboard(a_edge);
		case InputDevice::kMouse:
			return PlatformImGuiDetail::ObserveMouse(a_edge);
		case InputDevice::kGamepad:
			return PlatformImGuiDetail::ObserveGamepad(a_edge);
		}
		return false;
	}

	size_t ResetInput() noexcept
	{
		const PlatformImGuiDetail::ContextLock lock;
		const auto released = DearModdingUI::ReleaseHeldInput();
		const auto backendReady = PlatformImGuiDetail::Context().backend.load(std::memory_order_acquire) ==
			PlatformImGuiDetail::Backend::kReady;
		PlatformImGuiDetail::ApplyDrawingRequestLocked(
			backendReady && DearModdingUI::CurrentInputMode() != DearModdingUI::HostInputMode::kGameplay);
		if (!ImGui::GetCurrentContext())
			return released;
		auto& io = ImGui::GetIO();
		io.ClearInputKeys();
		io.ClearInputMouse();
		io.AddFocusEvent(DearModdingUI::EngineFocus::Active());
		return released;
	}

	void ObserveMouseMove() noexcept
	{
		const PlatformImGuiDetail::ContextLock lock;
		if (ImGui::GetCurrentContext() && DearModdingUI::IsMenuVisible())
			DearModdingUI::ControllerNavigation::UseCursor();
	}

	void ObserveStick(bool a_left, float a_x, float a_y) noexcept
	{
		const PlatformImGuiDetail::ContextLock lock;
		if (ImGui::GetCurrentContext() && DearModdingUI::IsMenuVisible() &&
			DearModdingUI::EngineFocus::Active())
			DearModdingUI::ControllerNavigation::QueueStick(a_left, a_x, a_y);
	}

	void ReleaseGamepad() noexcept
	{
		const PlatformImGuiDetail::ContextLock lock;
		(void)DearModdingUI::ReleaseHeldInput();
	}
}
