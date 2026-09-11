// CommonLib declarations must precede the Windows SDK macros.
#include <RE/C/ControlMap.h>
#include <RE/U/UI.h>
#include <REX/REX.h>

#include "PlatformImGuiInternal.h"

#include <Platform/input/CarrierMenu.h>
#include <Platform/input/CursorLoader.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/MenuDismissal.h>
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
		std::array<std::atomic<bool>, 256> s_consumedToggleKeys{};
		std::atomic<bool> s_consumedEscape{ false };
		bool s_previousIgnoreKeyboardMouse{ false };
		bool s_inputSuppressed{ false };

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

		void SetGameInputSuppressed(bool a_suppressed) noexcept
		{
			auto* controlMap = RE::ControlMap::GetSingleton();
			if (!controlMap)
			{
				if (!a_suppressed)
					s_inputSuppressed = false;
				return;
			}
			if (a_suppressed)
			{
				if (!s_inputSuppressed)
				{
					s_previousIgnoreKeyboardMouse =
						controlMap->ignoreKeyboardMouse;
					s_inputSuppressed = true;
				}
				controlMap->SetIgnoreKeyboardMouse(true);
			}
			else if (s_inputSuppressed)
			{
				controlMap->SetIgnoreKeyboardMouse(
					s_previousIgnoreKeyboardMouse);
				s_inputSuppressed = false;
			}
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

			const auto keyIndex = static_cast<size_t>(a_wparam);
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
						!focusLost && DearModdingUI::IsMenuVisible());
					if (ImGui::GetCurrentContext())
						ImGui::GetIO().AddFocusEvent(!focusLost);
				}
				inputFocused = !focusLost && DearModdingUI::CursorLoader::HasFocus();
			}
			const auto keyPressed =
				a_message == WM_KEYDOWN || a_message == WM_SYSKEYDOWN;
			const auto keyReleased =
				a_message == WM_KEYUP || a_message == WM_SYSKEYUP;
			const auto escapeDecision = DecideEscapeMessage(
				a_message,
				static_cast<uint32_t>(a_wparam),
				static_cast<uint64_t>(a_lparam),
				inputFocused && DearModdingUI::IsMenuVisible(),
				s_consumedEscape.load(std::memory_order_acquire));
			if (escapeDecision == EscapeMessageDecision::kCapture)
			{
				s_consumedEscape.store(true, std::memory_order_release);
				const ContextLock lock;
				DearModdingUI::CaptureMenuEscapePress(
					DearModdingUI::IsMenuVisible(),
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
				const auto* ui = RE::UI::GetSingleton();
				DearModdingUI::Hotkeys::SetContext({
					DearModdingUI::IsMenuVisible(),
					DearModdingUI::PresentationServices::
						HasActiveDialog(),
					DearModdingUI::IsMenuVisible(),
					ui && ui->menuMode == 0
				});
				uint32_t modifiers{ 0 };
				if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
					modifiers |=
						DearModdingUI::kHotkeyModifierShift;
				if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
					modifiers |=
						DearModdingUI::kHotkeyModifierControl;
				if ((GetKeyState(VK_MENU) & 0x8000) != 0)
					modifiers |= DearModdingUI::kHotkeyModifierAlt;

				if ((keyIndex == VK_F4) &&
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
					DearModdingUI::Hotkeys::HandleKey(
						static_cast<uint32_t>(a_wparam),
						modifiers,
						keyPressed,
						(static_cast<uint64_t>(a_lparam) &
							kKeyRepeatBit) != 0);
				if (hotkeyResult ==
					DearModdingUI::HotkeyMessageResult::
						kConsumedPairDropped)
				{
					REX::WARN(
						"DearModdingUI: hotkey event queue overflowed; one press/release pair was dropped"sv);
				}
				if (hotkeyResult !=
					DearModdingUI::HotkeyMessageResult::
						kPassThrough)
					return 0;
			}

			if (!escapeConsumed)
			{
				const auto trackableKey =
					keyIndex < s_consumedToggleKeys.size();
				const auto pressConsumed = trackableKey &&
					s_consumedToggleKeys[keyIndex].load(
						std::memory_order_acquire);
				const auto toggleDecision = DecideToggleMessage(
					a_message,
					static_cast<uint64_t>(a_lparam),
					pressConsumed);
				if (toggleDecision ==
					ToggleMessageDecision::kConsume)
					return 0;
				if (toggleDecision ==
					ToggleMessageDecision::kConsumeAndRelease)
				{
					s_consumedToggleKeys[keyIndex].store(
						false,
						std::memory_order_release);
					return 0;
				}
				if (toggleDecision ==
						ToggleMessageDecision::kDispatch &&
					inputFocused &&
					Context().callbacks.toggle(
						static_cast<uint32_t>(a_wparam)))
				{
					if (trackableKey)
					{
						s_consumedToggleKeys[keyIndex].store(
							true,
							std::memory_order_release);
					}
					return 0;
				}
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
		SetGameInputSuppressed(suppress);
		if (previous == active || !ImGui::GetCurrentContext())
			return;

		auto& io = ImGui::GetIO();
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
		SetModalInputStateLocked(false);
		DearModdingUI::CloseMenu();
		DearModdingUI::CarrierMenu::Handle(a_event);
		if (ImGui::GetCurrentContext())
			DearModdingUI::CursorLoader::PrepareFrame(false);
	}

	void ClearConsumedToggleKeysLocked() noexcept
	{
		for (auto& consumed : s_consumedToggleKeys)
			consumed.store(false, std::memory_order_release);
	}
}
