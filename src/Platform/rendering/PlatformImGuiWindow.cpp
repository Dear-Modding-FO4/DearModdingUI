// CommonLib declarations must precede the Windows SDK macros.
#include <RE/U/UI.h>
#include <REX/REX.h>

#include "PlatformImGuiInternal.h"

#include <Platform/input/CarrierMenu.h>
#include <Platform/input/CursorLoader.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/host/ControllerNavigation.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <Platform/input/GameInput.h>

#include <imgui/imgui.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>

namespace Addictol::platformImguiDetail
{
	using namespace std::literals;
	using namespace ImguiPlatform;

	namespace
	{
		static_assert(
			kKeyboardMessageFirst == WM_KEYFIRST &&
			kKeyboardMessageLast == WM_KEYLAST);
		static_assert(
			kMouseMessageFirst == WM_MOUSEFIRST &&
			kMouseMessageLast == WM_MOUSELAST);
		static_assert(kKeyDownMessage == WM_KEYDOWN);
		static_assert(kKeyUpMessage == WM_KEYUP);
		static_assert(kSysKeyDownMessage == WM_SYSKEYDOWN);
		static_assert(kSysKeyUpMessage == WM_SYSKEYUP);
		static_assert(kEscapeVirtualKey == VK_ESCAPE);
		static_assert(kWindowNcDestroyMessage == WM_NCDESTROY);

		struct WindowHookRecord
		{
			std::atomic<bool> claimed{ false };
			std::atomic<HWND> window{ nullptr };
			std::atomic<WNDPROC> previous{ nullptr };
			std::atomic<bool> unicode{ false };
		};

		constexpr size_t kWindowHookCapacity = 4;
		std::array<WindowHookRecord, kWindowHookCapacity> s_windowHooks{};
		std::atomic<bool> s_consumedEscape{ false };

		[[nodiscard]] DearModdingUI::HotkeyMessageResult HandleHotkey(
			uint32_t a_keyCode, uint32_t a_modifiers, bool a_pressed, bool a_repeat) noexcept
		{
			const auto* ui = RE::UI::GetSingleton();
			DearModdingUI::Hotkeys::SetContext(
				DearModdingUI::CurrentHotkeyContext(ui && ui->menuMode == 0));
			const auto result = DearModdingUI::Hotkeys::HandleKey(
				a_keyCode, a_modifiers, a_pressed, a_repeat);
			if (result == DearModdingUI::HotkeyMessageResult::kMenuToggle &&
				Context().callbacks.toggle)
				Context().callbacks.toggle();
			else if (result == DearModdingUI::HotkeyMessageResult::kConsumedPairDropped)
				REX::WARN(
					"DearModdingUI: hotkey event queue overflowed; one press/release pair was dropped"sv);
			return result;
		}

		[[nodiscard]] WindowHookRecord* FindWindowHook(
			HWND a_window) noexcept
		{
			for (auto& record : s_windowHooks)
			{
				if (record.window.load(std::memory_order_acquire) ==
					a_window)
					return std::addressof(record);
			}
			return nullptr;
		}

		void RetireWindowHook(
			WindowHookRecord& a_record,
			HWND a_window) noexcept
		{
			const ContextLock lock;
			auto expectedWindow = a_window;
			if (!a_record.window.compare_exchange_strong(
					expectedWindow,
					nullptr,
					std::memory_order_acq_rel))
				return;

			a_record.previous.store(nullptr, std::memory_order_release);
			a_record.unicode.store(false, std::memory_order_release);
			a_record.claimed.store(false, std::memory_order_release);

			if (Context().activeWindow.load(std::memory_order_acquire) !=
				a_window)
				return;
			RetireActiveAttachmentLocked(nullptr, a_window);
		}

		LRESULT CallPreviousWindowProc(
			HWND a_window,
			UINT a_message,
			WPARAM a_wparam,
			LPARAM a_lparam) noexcept
		{
			auto* record = FindWindowHook(a_window);
			const auto unicode = record ?
				record->unicode.load(std::memory_order_acquire) :
				IsWindowUnicode(a_window) != FALSE;
			const auto previous = record ?
				record->previous.load(std::memory_order_acquire) :
				nullptr;

			const auto result = previous ?
				(unicode ?
						CallWindowProcW(
							previous,
							a_window,
							a_message,
							a_wparam,
							a_lparam) :
						CallWindowProcA(
							previous,
							a_window,
							a_message,
							a_wparam,
							a_lparam)) :
				(unicode ?
						DefWindowProcW(
							a_window,
							a_message,
							a_wparam,
							a_lparam) :
						DefWindowProcA(
							a_window,
							a_message,
							a_wparam,
							a_lparam));
			if (record && RetiresWindowHook(a_message))
				RetireWindowHook(*record, a_window);
			return result;
		}

		LRESULT CALLBACK HKWindowProc(
			HWND a_window,
			UINT a_message,
			WPARAM a_wparam,
			LPARAM a_lparam) noexcept
		{
			static thread_local bool reentered{ false };
			if (reentered ||
				a_window !=
					Context().activeWindow.load(
						std::memory_order_acquire))
			{
				return CallPreviousWindowProc(
					a_window,
					a_message,
					a_wparam,
					a_lparam);
			}

			struct ReentryGuard
			{
				explicit ReentryGuard(bool& a_value) noexcept :
					value(a_value)
				{
					value = true;
				}

				~ReentryGuard() noexcept
				{
					value = false;
				}

				bool& value;
			};

			uint32_t keyCode{};
			if (a_message == WM_KEYDOWN || a_message == WM_SYSKEYDOWN ||
				a_message == WM_KEYUP || a_message == WM_SYSKEYUP)
			{
				auto scanCode = static_cast<uint8_t>(
					(static_cast<uint64_t>(a_lparam) >> 16) & 0xFFu);
				auto extended =
					(static_cast<uint64_t>(a_lparam) & (1ull << 24)) != 0;
				if (!scanCode)
				{
					const auto mapped = MapVirtualKeyW(
						static_cast<UINT>(a_wparam), MAPVK_VK_TO_VSC_EX);
					scanCode = static_cast<uint8_t>(mapped & 0xFFu);
					const auto prefix = (mapped >> 8) & 0xFFu;
					extended = prefix == 0xE0 || prefix == 0xE1;
				}
				keyCode = KeyboardKeyCode(
					scanCode, extended, static_cast<uint32_t>(a_wparam));
			}
			const auto focusLost = a_message == WM_KILLFOCUS ||
				(a_message == WM_ACTIVATEAPP && !a_wparam);
			const auto focusGained = a_message == WM_SETFOCUS ||
				(a_message == WM_ACTIVATEAPP && a_wparam);
			bool inputFocused{ false };
			{
				const ContextLock lock;
				if (focusLost || focusGained)
				{
					if (focusLost)
						DearModdingUI::Hotkeys::ReleaseActiveKeys();
					ApplyDrawingRequestLocked(
						!focusLost &&
						DearModdingUI::CurrentInputMode() != DearModdingUI::HostInputMode::kGameplay);
					if (ImGui::GetCurrentContext())
						ImGui::GetIO().AddFocusEvent(!focusLost);
				}
				inputFocused = !focusLost && DearModdingUI::CursorLoader::HasFocus();
			}
			const auto keyPressed =
				a_message == WM_KEYDOWN || a_message == WM_SYSKEYDOWN;
			const auto keyReleased =
				a_message == WM_KEYUP || a_message == WM_SYSKEYUP;
			uint32_t modifiers{ 0 };
			if (keyPressed || keyReleased)
			{
				if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
					modifiers |= DearModdingUI::kHotkeyModifierShift;
				if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
					modifiers |= DearModdingUI::kHotkeyModifierControl;
				if ((GetKeyState(VK_MENU) & 0x8000) != 0)
					modifiers |= DearModdingUI::kHotkeyModifierAlt;
			}
			const auto escapeDecision = DecideEscapeMessage(
				a_message,
				static_cast<uint32_t>(a_wparam),
				static_cast<uint64_t>(a_lparam),
				inputFocused &&
					DearModdingUI::CurrentInputMode() != DearModdingUI::HostInputMode::kGameplay,
				s_consumedEscape.load(std::memory_order_acquire));
			if (escapeDecision == EscapeMessageDecision::kCapture)
			{
				s_consumedEscape.store(true, std::memory_order_release);
				const ContextLock lock;
				DearModdingUI::CaptureMenuEscapePress(
					DearModdingUI::CurrentInputMode(),
					DearModdingUI::PresentationServices::
						HasActiveDialog(),
					DearModdingUI::PresentationServices::
						ActiveDialogPopupId());
			}
			else if (
				escapeDecision ==
					EscapeMessageDecision::kConsumeAndRelease ||
				escapeDecision ==
					EscapeMessageDecision::kReleaseAndForward)
			{
				s_consumedEscape.store(false, std::memory_order_release);
			}
			const auto escapeConsumed =
				escapeDecision != EscapeMessageDecision::kForward &&
				escapeDecision !=
					EscapeMessageDecision::kReleaseAndForward;

			if (!escapeConsumed &&
				((inputFocused && keyPressed) || keyReleased))
			{
				if ((a_wparam == VK_F4) &&
					(modifiers &
						DearModdingUI::kHotkeyModifierAlt) &&
					!((modifiers &
							DearModdingUI::
								kHotkeyModifierControl) ||
						(modifiers &
							DearModdingUI::
								kHotkeyModifierShift)))
				{
					CallPreviousWindowProc(
						a_window,
						a_message,
						a_wparam,
						a_lparam);
				}

				const auto hotkeyResult =
					HandleHotkey(
						keyCode,
						modifiers,
						keyPressed,
						(static_cast<uint64_t>(a_lparam) &
							kKeyRepeatBit) != 0);
				if (hotkeyResult !=
					DearModdingUI::HotkeyMessageResult::
						kPassThrough)
					return 0;
			}

			{
				const ContextLock lock;
				if (DearModdingUI::CursorLoader::HandleWindowMessage(
						a_window,
						a_message,
						static_cast<uint64_t>(a_lparam)))
					return 1;
			}

			auto& context = Context();
			if (!context.drawingEnabled.load(
					std::memory_order_acquire) ||
				context.backend.load(std::memory_order_acquire) !=
					Backend::kReady)
			{
				return escapeConsumed ?
					0 :
					CallPreviousWindowProc(
						a_window,
						a_message,
						a_wparam,
						a_lparam);
			}

			BackendMessageResult backendResult;
			{
				const ContextLock lock;
				const ReentryGuard reentry{ reentered };
				backendResult = HandleBackendWindowMessageLocked(
					a_window,
					a_message,
					a_wparam,
					a_lparam,
					escapeConsumed);
			}
			return backendResult.handled && backendResult.swallow ?
				backendResult.result :
				CallPreviousWindowProc(
					a_window,
					a_message,
					a_wparam,
					a_lparam);
		}
	}

	bool HasWindowHook(HWND a_window) noexcept
	{
		return FindWindowHook(a_window) != nullptr;
	}

	bool SubclassWindowLocked(HWND a_window) noexcept
	{
		auto& context = Context();
		if (FindWindowHook(a_window))
		{
			context.windowReady.store(
				true,
				std::memory_order_release);
			return true;
		}

		WindowHookRecord* record{ nullptr };
		for (auto& candidate : s_windowHooks)
		{
			bool expected{ false };
			if (candidate.claimed.compare_exchange_strong(
					expected,
					true,
					std::memory_order_acq_rel))
			{
				record = std::addressof(candidate);
				break;
			}
		}
		if (!record)
		{
			REX::ERROR(
				"Platform Imgui: window hook capacity exhausted"sv);
			return false;
		}

		const auto unicode = IsWindowUnicode(a_window) != FALSE;
		const auto current = unicode ?
			GetWindowLongPtrW(a_window, GWLP_WNDPROC) :
			GetWindowLongPtrA(a_window, GWLP_WNDPROC);
		if (reinterpret_cast<WNDPROC>(current) == &HKWindowProc)
		{
			record->claimed.store(false, std::memory_order_release);
			REX::ERROR(
				"Platform Imgui: window hook predecessor is unavailable"sv);
			return false;
		}

		record->unicode.store(unicode, std::memory_order_relaxed);
		record->previous.store(
			reinterpret_cast<WNDPROC>(current),
			std::memory_order_release);
		record->window.store(a_window, std::memory_order_release);

		SetLastError(0);
		const auto previous = unicode ?
			SetWindowLongPtrW(
				a_window,
				GWLP_WNDPROC,
				reinterpret_cast<LONG_PTR>(&HKWindowProc)) :
			SetWindowLongPtrA(
				a_window,
				GWLP_WNDPROC,
				reinterpret_cast<LONG_PTR>(&HKWindowProc));
		if (!previous && GetLastError() != 0)
		{
			const auto error = GetLastError();
			record->window.store(nullptr, std::memory_order_release);
			record->previous.store(nullptr, std::memory_order_release);
			record->unicode.store(false, std::memory_order_release);
			record->claimed.store(false, std::memory_order_release);
			REX::WARN(
				"Platform Imgui: window subclassing failed with error {}"sv,
				error);
			return false;
		}

		record->previous.store(
			reinterpret_cast<WNDPROC>(previous),
			std::memory_order_release);
		context.windowReady.store(true, std::memory_order_release);
		return true;
	}

	void SetModalInputStateLocked(bool a_visible) noexcept
	{
		auto& context = Context();
		const auto active = a_visible && DearModdingUI::CursorLoader::HasFocus();
		const auto previous = context.drawingEnabled.exchange(
			active,
			std::memory_order_acq_rel);
		const auto suppress = ShouldSuppressGameInput(active);
		GameInput::SetBlocked(suppress);
		if (previous != active)
			ResetGameCursorWaitLocked();
		if (previous == active || !ImGui::GetCurrentContext())
			return;

		auto& io = ImGui::GetIO();
		if (!active)
			DearModdingUI::ControllerNavigation::Reset();
		io.ClearInputKeys();
		io.ClearInputMouse();
	}

	void ApplyDrawingRequestLocked(bool a_enabled) noexcept
	{
		SetModalInputStateLocked(a_enabled);
		const auto active = Context().drawingEnabled.load(std::memory_order_acquire);
		DearModdingUI::CarrierMenu::Handle(
			active ?
				DearModdingUI::CarrierMenu::Event::kOpen :
				DearModdingUI::CarrierMenu::Event::kClose);
		if (ImGui::GetCurrentContext())
			DearModdingUI::CursorLoader::PrepareFrame(active);
	}

	void CloseModalStateLocked(
		DearModdingUI::CarrierMenu::Event a_event) noexcept
	{
		DearModdingUI::Hotkeys::ReleaseActiveKeys();
		SetModalInputStateLocked(false);
		DearModdingUI::EndOverlayFocus(
			a_event == DearModdingUI::CarrierMenu::Event::kShutdown ?
				DMUI_OVERLAY_FOCUS_END_HOST_UNAVAILABLE :
				DMUI_OVERLAY_FOCUS_END_INTERRUPTED);
		DearModdingUI::CloseMenu();
		DearModdingUI::CarrierMenu::Handle(a_event);
		if (ImGui::GetCurrentContext())
			DearModdingUI::CursorLoader::PrepareFrame(false);
	}

}

namespace Addictol::PlatformImgui
{
	void ObserveButton(uint32_t a_keyCode, bool a_pressed, bool a_repeat, bool a_pulse, float a_value) noexcept
	{
		{
			const platformImguiDetail::ContextLock lock;
			if (a_pressed && !DearModdingUI::CursorLoader::HasFocus())
				return;
		}
		if (DearModdingUI::ControllerNavigation::ButtonKey(a_keyCode) != ImGuiKey_None)
		{
			DearModdingUI::HotkeyMessageResult result;
			{
				const platformImguiDetail::ContextLock lock;
				const auto* ui = RE::UI::GetSingleton();
				result = DearModdingUI::ControllerNavigation::RouteButton(
					a_keyCode, a_pressed, a_repeat,
					a_keyCode >= DearModdingUI::KeyCatalog::kPadLT ?
						DearModdingUI::ControllerNavigation::AnalogValue(a_value) : a_value,
					DearModdingUI::CurrentHotkeyContext(ui && ui->menuMode == 0));
			}
			// Toggle callbacks reacquire the platform lock.
			if (result == DearModdingUI::HotkeyMessageResult::kMenuToggle &&
				platformImguiDetail::Context().callbacks.toggle)
				platformImguiDetail::Context().callbacks.toggle();
			return;
		}
		uint32_t modifiers = 0;
		if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
			modifiers |= DearModdingUI::kHotkeyModifierShift;
		if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0)
			modifiers |= DearModdingUI::kHotkeyModifierControl;
		if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0)
			modifiers |= DearModdingUI::kHotkeyModifierAlt;
		(void)platformImguiDetail::HandleHotkey(a_keyCode, modifiers, a_pressed, a_repeat);
		if (a_pulse && a_pressed)
			(void)platformImguiDetail::HandleHotkey(a_keyCode, modifiers, false, false);
	}

	void ObserveMouseMove() noexcept
	{
		const platformImguiDetail::ContextLock lock;
		if (ImGui::GetCurrentContext() && DearModdingUI::IsMenuVisible())
			DearModdingUI::ControllerNavigation::UseCursor();
	}

	void ObserveStick(bool a_left, float a_x, float a_y) noexcept
	{
		const platformImguiDetail::ContextLock lock;
		if (ImGui::GetCurrentContext() && DearModdingUI::IsMenuVisible() &&
			DearModdingUI::CursorLoader::HasFocus())
			DearModdingUI::ControllerNavigation::QueueStick(a_left, a_x, a_y);
	}

	void ReleaseGamepad() noexcept
	{
		const platformImguiDetail::ContextLock lock;
		DearModdingUI::ControllerNavigation::Reset();
		DearModdingUI::Hotkeys::ReleaseActiveKeys();
	}
}
