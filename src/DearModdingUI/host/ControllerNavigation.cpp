#include <DearModdingUI/host/ControllerNavigation.h>

#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/host/ModalCoordinator.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/presentation/Theme.h>
#include <REX/REX.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace DearModdingUI::ControllerNavigation
{
	namespace
	{
		ControllerMode s_mode{ ControllerMode::kCursor };
		std::array<bool, kButtonKeys.size()> s_down{};
		std::array<bool, kButtonKeys.size()> s_forwarded{};
		std::array<ImGuiKeyData, kButtonKeys.size()> s_desktopButtons{};
		ImVec2 s_cursor{ -FLT_MAX, -FLT_MAX };
		ImVec2 s_observedCursor{ -FLT_MAX, -FLT_MAX };
		ImVec2 s_rightStick{};
		std::array<float, 4> s_leftStick{};
		std::optional<ImVec2> s_cursorWarp;
		bool s_mouseA{};
		bool s_search{};
		bool s_close{};
		bool s_reset{};
		bool s_focusSelected{};
		bool s_paneRequested{};
		bool s_hasNavigated{};
		uint32_t s_lastEvent{};
		Pane s_requestedPane{ Pane::kSidebar };
		Pane s_drawingPane{ Pane::kContent };
		std::array<ImGuiWindow*, 2> s_paneWindows{};

		void SeedLeftStick() noexcept
		{
			// Remapped keys are not retained by ImGui, so seed them before producer deduplication.
			for (size_t slot = 0; slot < s_leftStick.size(); ++slot)
				ImGui::GetIO().AddKeyAnalogEvent(static_cast<ImGuiKey>(ImGuiKey_GamepadLStickLeft + slot),
					s_leftStick[slot] > 0.0f, s_leftStick[slot]);
		}

		void RequestPane(Pane a_pane) noexcept
		{
			s_requestedPane = a_pane;
			s_paneRequested = !ModalCoordinator::HasModal() &&
				!ImGui::GetCurrentContext()->OpenPopupStack.Size;
		}

		void SetMode(ControllerMode a_mode) noexcept
		{
			if (s_mode != a_mode)
				s_cursorWarp.reset();
			s_mode = a_mode;
			if (a_mode == ControllerMode::kCursor && ImGui::GetCurrentContext())
			{
				ImGui::SetNavCursorVisible(false);
				ImGui::GetCurrentContext()->NavHighlightItemUnderNav = false;
			}
		}

		void Back(HostInputMode a_inputMode) noexcept
		{
			CaptureMenuEscapePress(a_inputMode, PresentationServices::HasActiveDialog(),
				PresentationServices::ActiveDialogPopupId());
		}

		[[nodiscard]] auto HintChips() noexcept
		{
			const auto editing = ImGui::GetCurrentContext()->ActiveId != 0;
			return std::array{
				std::pair{ "A", editing ? "Confirm" : "Select" }, std::pair{ "B", "Back" },
				std::pair{ "X", "Reset" }, std::pair{ "Y", "Search" },
				std::pair{ "LB/RB", editing ? "Adjust" : "Pane" },
				std::pair{ "LT/RT", "Page" }, std::pair{ "LS", "Cursor" }, std::pair{ "RS", "Scroll" },
				std::pair{ "Start", "Close" }
			};
		}

		struct HintMetrics
		{
			float keyWidth;
			float labelGap;
			float width;
		};

		// Tight key-to-label and wide group gaps keep each label visually owned by its key.
		[[nodiscard]] float HintGroupGap() noexcept
		{
			return ImGui::GetFontSize() * 1.25f;
		}

		[[nodiscard]] HintMetrics MeasureHint(
			const std::pair<const char*, const char*>& a_chip) noexcept
		{
			const auto& style = ImGui::GetStyle();
			const auto keyWidth = ImGui::CalcTextSize(a_chip.first).x + style.FramePadding.x * 2.0f;
			const auto labelGap = style.ItemInnerSpacing.x * 0.75f;
			return { keyWidth, labelGap,
				keyWidth + labelGap + ImGui::CalcTextSize(a_chip.second).x };
		}
	}

	HotkeyMessageResult RouteButton(
		uint32_t a_code, bool a_pressed, bool a_repeat, float a_value,
		HotkeyContextState a_context) noexcept
	{
		if (ButtonKey(a_code) == ImGuiKey_None)
			return HotkeyMessageResult::kPassThrough;
		Hotkeys::SetContext(a_context);
		const auto capturing = Hotkeys::IsCapturing();
		const auto shell = a_context.inputMode == HostInputMode::kShell;
		const auto back = a_code == KeyCatalog::kPadB && a_context.inputMode != HostInputMode::kGameplay;
		// A fresh B press retires any gameplay-owned B; holds keep the claim.
		const auto result = back && a_pressed && a_repeat ? HotkeyMessageResult::kPassThrough :
			Hotkeys::HandleKey(a_code, 0, a_pressed && !back, a_repeat);
		if (result == HotkeyMessageResult::kConsumedPairDropped)
			REX::WARN("DearModdingUI: hotkey event queue overflowed; one press/release pair was dropped");
		if (!ImGui::GetCurrentContext())
			return result;
		if (result == HotkeyMessageResult::kMenuToggle && !shell)
			UseNavigation();
		auto& forwarded = s_forwarded[a_code - KeyCatalog::kGamepadButtonOffset];
		const auto down = a_pressed && shell && !capturing &&
			result == HotkeyMessageResult::kPassThrough && (forwarded || !a_repeat);
		if (back)
		{
			if (a_pressed && !a_repeat && result == HotkeyMessageResult::kPassThrough)
			{
				SetMode(DecideControllerMode(s_mode, false, a_code));
				Back(a_context.inputMode);
				Hotkeys::Claim(a_code);
			}
		}
		else if (down || forwarded)
			QueueButton(a_code, down ? a_value : 0.0f);
		forwarded = down && !back;
		return result;
	}

	void QueueButton(uint32_t a_code, float a_value) noexcept
	{
		const auto key = ButtonKey(a_code);
		if (key == ImGuiKey_None)
			return;
		auto& io = ImGui::GetIO();
		if (a_code >= KeyCatalog::kPadUp && a_code <= KeyCatalog::kPadRight)
		{
			const auto down = s_down[a_code - KeyCatalog::kGamepadButtonOffset];
			// A synthetic stick hold must not hide a physical D-pad edge.
			io.AddKeyEvent(key, down);
		}
		if (a_code >= KeyCatalog::kPadLT)
			io.AddKeyAnalogEvent(key, a_value > 0.0f, a_value);
		else
			io.AddKeyEvent(key, a_value > 0.0f);
	}

	void QueueStick(bool a_left, float a_x, float a_y) noexcept
	{
		const ImVec2 value{
			std::copysign(AnalogValue(std::abs(a_x), 0.2f), a_x),
			std::copysign(AnalogValue(std::abs(a_y), 0.2f), a_y)
		};
		auto& io = ImGui::GetIO();
		if (a_left)
			SeedLeftStick();
		const auto axis = [&](ImGuiKey a_key, float a_amount) {
			const auto amount = (std::max)(a_amount, 0.0f);
			io.AddKeyAnalogEvent(a_key, amount > 0.0f, amount);
		};
		axis(a_left ? ImGuiKey_GamepadLStickLeft : ImGuiKey_GamepadRStickLeft, -value.x);
		axis(a_left ? ImGuiKey_GamepadLStickRight : ImGuiKey_GamepadRStickRight, value.x);
		axis(a_left ? ImGuiKey_GamepadLStickUp : ImGuiKey_GamepadRStickUp, value.y);
		axis(a_left ? ImGuiKey_GamepadLStickDown : ImGuiKey_GamepadRStickDown, -value.y);
	}

	void ObserveCursor(float a_x, float a_y) noexcept
	{
		if (s_observedCursor.x != -FLT_MAX &&
			(std::abs(a_x - s_observedCursor.x) > 0.5f || std::abs(a_y - s_observedCursor.y) > 0.5f))
		{
			SetMode(ControllerMode::kCursor);
			s_cursorWarp.reset();
			s_cursor = { a_x, a_y };
		}
		else if (s_cursor.x == -FLT_MAX)
			s_cursor = { a_x, a_y };
		s_observedCursor = { a_x, a_y };
	}

	void UseCursor() noexcept
	{
		SetMode(ControllerMode::kCursor);
		s_cursorWarp.reset();
	}

	void UseNavigation() noexcept
	{
		SetMode(ControllerMode::kNavigation);
		if (!s_hasNavigated)
		{
			RequestPane(Pane::kSidebar);
			s_hasNavigated = true;
		}
	}
	bool IsNavigating() noexcept { return s_mode == ControllerMode::kNavigation; }
	std::optional<ImVec2> TakeCursorWarp() noexcept { return std::exchange(s_cursorWarp, std::nullopt); }

	void Reset() noexcept
	{
		s_mode = ControllerMode::kCursor;
		s_down.fill(false);
		s_forwarded.fill(false);
		s_cursor = s_observedCursor = { -FLT_MAX, -FLT_MAX };
		s_rightStick = {};
		s_leftStick.fill(0.0f);
		s_cursorWarp.reset();
		s_lastEvent = 0;
		s_mouseA = s_search = s_close = s_reset = s_focusSelected = s_paneRequested = false;
		s_hasNavigated = false;
		if (ImGui::GetCurrentContext())
		{
			auto& io = ImGui::GetIO();
			auto& events = ImGui::GetCurrentContext()->InputEventsQueue;
			for (int index = events.Size - 1; index >= 0; --index)
			{
				if (events[index].Type == ImGuiInputEventType_Key &&
					ImGui::IsGamepadKey(events[index].Key.Key))
					events.erase(events.Data + index);
			}
			for (int key = ImGuiKey_GamepadStart; key <= ImGuiKey_GamepadRStickDown; ++key)
				io.AddKeyEvent(static_cast<ImGuiKey>(key), false);
			io.AddMouseButtonEvent(0, false);
			io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			io.ConfigNavCursorVisibleAuto = true;
			ImGui::SetNavCursorVisible(false);
			ImGui::GetCurrentContext()->NavHighlightItemUnderNav = false;
			s_lastEvent = ImGui::GetCurrentContext()->InputEventsNextEventId - 1;
		}
	}

	void BeginDesktopInput() noexcept
	{
		// Seed physical state so backend deduplication cannot discard an owned button's release.
		auto& io = ImGui::GetIO();
		for (size_t slot = 0; slot < kButtonKeys.size(); ++slot)
			io.AddKeyAnalogEvent(kButtonKeys[slot],
				s_desktopButtons[slot].Down, s_desktopButtons[slot].AnalogValue);
		SeedLeftStick();
	}

	bool PrepareFrame(HostInputMode a_inputMode, bool a_desktop) noexcept
	{
		auto inputMode = a_inputMode;
		auto& g = *ImGui::GetCurrentContext();
		auto& io = g.IO;
		bool toggle = false;
		if (a_desktop)
		{
			if (!(io.BackendFlags & ImGuiBackendFlags_HasGamepad))
			{
				s_desktopButtons.fill({});
				s_leftStick.fill(0.0f);
			}
			// Only backend events enter routing; newly queued navigation events are already routed.
			const auto lastBackendEvent = g.InputEventsNextEventId - 1;
			for (int index = 0; index < g.InputEventsQueue.Size;)
			{
				const auto event = g.InputEventsQueue[index];
				if (event.EventId > lastBackendEvent)
					break;
				const auto found = event.Type == ImGuiInputEventType_Key ?
					std::find(kButtonKeys.begin(), kButtonKeys.end(), event.Key.Key) : kButtonKeys.end();
				if (event.EventId <= s_lastEvent || found == kButtonKeys.end())
				{
					++index;
					continue;
				}
				const auto slot = static_cast<size_t>(found - kButtonKeys.begin());
				g.InputEventsQueue.erase(g.InputEventsQueue.Data + index);
				if (!(io.BackendFlags & ImGuiBackendFlags_HasGamepad))
					continue;
				auto& physical = s_desktopButtons[slot];
				const auto repeat = event.Key.Down && physical.Down;
				physical.Down = event.Key.Down;
				physical.AnalogValue = event.Key.AnalogValue;
				const auto result = RouteButton(
					static_cast<uint32_t>(slot) + KeyCatalog::kGamepadButtonOffset,
					event.Key.Down, repeat, event.Key.AnalogValue,
					{ inputMode, PresentationServices::HasActiveDialog(), false });
				if (result == HotkeyMessageResult::kMenuToggle)
				{
					toggle = !toggle;
					inputMode = inputMode == HostInputMode::kShell ?
						HostInputMode::kGameplay : HostInputMode::kShell;
				}
			}
		}
		if (inputMode != HostInputMode::kShell)
		{
			// Reset queues a mouse release, which would cancel every click on a focused overlay.
			if (inputMode == HostInputMode::kGameplay)
				Reset();
			return toggle;
		}
		s_reset = false;
		// Own commands before NewFrame so ImGui cannot also dismiss or activate a surface.
		for (int index = 0; index < g.InputEventsQueue.Size;)
		{
			const auto event = g.InputEventsQueue[index];
			if (event.EventId <= s_lastEvent)
			{
				++index;
				continue;
			}
			s_lastEvent = event.EventId;
			if (a_desktop && event.Type == ImGuiInputEventType_MousePos)
			{
				ObserveCursor(event.MousePos.PosX, event.MousePos.PosY);
				// Backend polling of an unmoved OS cursor must not undo a virtual cursor warp.
				g.InputEventsQueue[index].MousePos.PosX = s_cursor.x;
				g.InputEventsQueue[index].MousePos.PosY = s_cursor.y;
			}
			if (event.Type != ImGuiInputEventType_Key || event.Key.Key < ImGuiKey_GamepadStart ||
				event.Key.Key > ImGuiKey_GamepadRStickDown)
			{
				++index;
				continue;
			}
			const auto key = static_cast<ImGuiKey>(event.Key.Key);
			const auto found = std::find(kButtonKeys.begin(), kButtonKeys.end(), key);
			bool remove = false;
			if (found != kButtonKeys.end())
			{
				const auto slot = static_cast<size_t>(found - kButtonKeys.begin());
				const auto code = static_cast<uint32_t>(slot) + KeyCatalog::kGamepadButtonOffset;
				const auto pressed = event.Key.Down && !s_down[slot];
				s_down[slot] = event.Key.Down;
				if (pressed)
				{
					SetMode(DecideControllerMode(s_mode, false, code));
					if (code >= KeyCatalog::kPadUp && code <= KeyCatalog::kPadRight)
						UseNavigation();
					if (TogglesControllerMode(code))
					{
						if (IsNavigating())
							UseNavigation();
						else if (g.NavWindow && g.NavId)
							s_cursorWarp = ImGui::WindowRectRelToAbs(
								g.NavWindow, g.NavWindow->NavRectRel[g.NavLayer]).GetCenter();
					}
				}
				if (code >= KeyCatalog::kPadUp && code <= KeyCatalog::kPadRight)
				{
					g.InputEventsQueue.erase(g.InputEventsQueue.Data + index);
					continue;
				}
				const auto action = DecideControllerAction(code, s_mode, g.ActiveId != 0, false);
				if (code == KeyCatalog::kPadX)
				{
					s_reset |= pressed && !g.ActiveId;
					remove = true;
				}
				else if (code == KeyCatalog::kPadA)
				{
					if (pressed)
						s_mouseA = action == ControllerAction::kCursorClick;
					if (s_mouseA)
					{
						io.AddMouseButtonEvent(0, event.Key.Down);
						remove = true;
						if (!event.Key.Down)
							s_mouseA = false;
					}
				}
				else if (action != ControllerAction::kNative)
				{
					if (pressed)
					{
						switch (action)
						{
						case ControllerAction::kClose: s_close = true; break;
						case ControllerAction::kSearch: s_search = true; break;
						case ControllerAction::kSidebar:
						case ControllerAction::kContent:
							RequestPane(action == ControllerAction::kSidebar ? Pane::kSidebar : Pane::kContent);
							break;
						default: break;
						}
					}
					remove = true;
				}
			}
			else
			{
				if (key <= ImGuiKey_GamepadLStickDown)
				{
					s_leftStick[key - ImGuiKey_GamepadLStickLeft] = event.Key.AnalogValue;
					g.InputEventsQueue.erase(g.InputEventsQueue.Data + index);
					continue;
				}
				auto& stick = s_rightStick;
				switch (key)
				{
				case ImGuiKey_GamepadLStickLeft: case ImGuiKey_GamepadRStickLeft: stick.x = -event.Key.AnalogValue; break;
				case ImGuiKey_GamepadLStickRight: case ImGuiKey_GamepadRStickRight: if (event.Key.AnalogValue || stick.x > 0) stick.x = event.Key.AnalogValue; break;
				case ImGuiKey_GamepadLStickUp: case ImGuiKey_GamepadRStickUp: stick.y = event.Key.AnalogValue; break;
				case ImGuiKey_GamepadLStickDown: case ImGuiKey_GamepadRStickDown: if (event.Key.AnalogValue || stick.y < 0) stick.y = -event.Key.AnalogValue; break;
				default: break;
				}
				g.InputEventsQueue[index].Key.AnalogValue = 0.0f;
				remove = true;
			}
			if (remove)
				ImGui::SetKeyOwner(key, ImGuiKeyOwner_Any, ImGuiInputFlags_LockUntilRelease);
			++index;
		}
		// This ImGui version moves focus and edits through D-pad keys; LStick keys only scroll.
		const std::array stickDirections{ s_leftStick[2], s_leftStick[3], s_leftStick[0], s_leftStick[1] };
		for (size_t slot = 0; slot < stickDirections.size(); ++slot)
		{
			const auto value = IsNavigating() && !Hotkeys::IsCapturing() ?
				(s_down[slot] ? 1.0f : stickDirections[slot]) : 0.0f;
			io.AddKeyAnalogEvent(kButtonKeys[slot], value > 0.0f, value);
		}
		if (a_desktop && s_cursorWarp)
		{
			s_cursor = *TakeCursorWarp();
			for (int index = g.InputEventsQueue.Size - 1; index >= 0; --index)
				if (g.InputEventsQueue[index].Type == ImGuiInputEventType_MousePos)
					g.InputEventsQueue.erase(g.InputEventsQueue.Data + index);
			io.MousePos = s_cursor;
			io.AddMousePosEvent(s_cursor.x, s_cursor.y);
		}
		if (a_desktop && !IsNavigating())
		{
			const auto stick = s_rightStick;
			if (stick.x || stick.y)
			{
				const auto delta = io.DeltaTime * ImGui::GetFontSize() * 24.0f;
				if (s_cursor.x == -FLT_MAX)
					s_cursor = { io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f };
				s_cursor.x = std::clamp(s_cursor.x + stick.x * delta, 0.0f, io.DisplaySize.x);
				s_cursor.y = std::clamp(s_cursor.y - stick.y * delta, 0.0f, io.DisplaySize.y);
				io.AddMousePosEvent(s_cursor.x, s_cursor.y);
			}
		}
		s_lastEvent = g.InputEventsNextEventId - 1;
		io.ConfigNavCursorVisibleAuto = !IsNavigating();
		if (IsNavigating() && !s_mouseA)
			io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
		else
			io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		if (IsNavigating())
		{
			ImGui::SetNavCursorVisible(true);
			g.NavHighlightItemUnderNav = true;
		}
		return toggle;
	}

	void BeginShell() noexcept
	{
		auto& g = *ImGui::GetCurrentContext();
		if (IsNavigating())
		{
			ImGui::SetNavCursorVisible(true);
			g.NavHighlightItemUnderNav = true;
		}
		auto* window = IsNavigating() ? g.NavWindow : g.HoveredWindow;
		if (!window || Hotkeys::IsCapturing())
			return;
		const auto step = ImGui::GetFontSize() * g.IO.DeltaTime * 30.0f;
		// Cursor mode follows the game: the right stick moves the cursor, so the left stick scrolls.
		const auto scrollStick = IsNavigating() ? s_rightStick :
			ImVec2{ s_leftStick[1] - s_leftStick[0], s_leftStick[2] - s_leftStick[3] };
		auto scroll = -scrollStick.y * step;
		if (!g.ActiveId)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_GamepadL2))
				scroll -= window->InnerRect.GetHeight() * 0.8f;
			if (ImGui::IsKeyPressed(ImGuiKey_GamepadR2))
				scroll += window->InnerRect.GetHeight() * 0.8f;
		}
		if (scroll)
			ImGui::SetScrollY(window, std::clamp(window->Scroll.y + scroll, 0.0f, window->ScrollMax.y));
		if (scrollStick.x)
			ImGui::SetScrollX(window, std::clamp(window->Scroll.x + scrollStick.x * step, 0.0f, window->ScrollMax.x));
	}

	void BeginPane(Pane a_pane) noexcept
	{
		s_drawingPane = a_pane;
		s_paneWindows[static_cast<size_t>(a_pane)] = ImGui::GetCurrentWindow();
		if (!s_paneRequested || s_requestedPane != a_pane ||
			ImGui::GetCurrentContext()->OpenPopupStack.Size || ModalCoordinator::HasModal())
			return;
		auto* window = ImGui::GetCurrentWindow();
		ImGui::FocusWindow(window);
		ImGui::NavInitWindow(window, true);
		s_focusSelected = a_pane == Pane::kSidebar;
		s_paneRequested = false;
	}

	void EndPanes() noexcept
	{
		auto& g = *ImGui::GetCurrentContext();
		const auto from = [&](Pane a_pane) {
			auto* pane = s_paneWindows[static_cast<size_t>(a_pane)];
			return pane && g.NavWindow && ImGui::IsWindowChildOf(g.NavWindow, pane, false, false);
		};
		// A horizontal move that finds nothing at a pane edge continues into the neighboring pane.
		if (!IsNavigating() || g.ActiveId || g.OpenPopupStack.Size || ModalCoordinator::HasModal() ||
			!ImGui::NavMoveRequestButNoResultYet())
			return;
		if (g.NavMoveDir == ImGuiDir_Left && from(Pane::kContent))
			RequestPane(Pane::kSidebar);
		else if (g.NavMoveDir == ImGuiDir_Right && from(Pane::kSidebar))
			RequestPane(Pane::kContent);
	}

	void FocusSelectedItem(bool a_selected) noexcept
	{
		if (!s_focusSelected || s_drawingPane != Pane::kSidebar || !a_selected)
			return;
		ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
		ImGui::FocusWindow(ImGui::GetCurrentWindow());
		auto& g = *ImGui::GetCurrentContext();
		g.NavInitRequest = false;
		g.NavInitResult.Clear();
		ImGui::NavMoveRequestCancel();
		ImGui::SetScrollHereY();
		s_focusSelected = false;
	}

	bool TakeSearch() noexcept { return std::exchange(s_search, false); }
	bool TakeClose() noexcept { return std::exchange(s_close, false); }

	bool ResetFocusedRow(bool a_available, const ImVec2& a_min, const ImVec2& a_max) noexcept
	{
		const auto& g = *ImGui::GetCurrentContext();
		if (!IsNavigating() || g.NavWindow != ImGui::GetCurrentWindow() || !g.NavId)
			return false;
		const auto rect = ImGui::WindowRectRelToAbs(
			g.NavWindow, g.NavWindow->NavRectRel[g.NavLayer]);
		const auto focused = ImRect{ a_min, a_max }.Contains(rect.GetCenter());
		if (focused && DecideControllerAction(KeyCatalog::kPadX, s_mode, g.ActiveId != 0, a_available) ==
			ControllerAction::kReset)
			return std::exchange(s_reset, false);
		return false;
	}

	float HintsWidth() noexcept
	{
		if (!IsNavigating())
			return 0.0f;
		const Theme::FontGuard font{ Theme::FontRole::kSubtext };
		const auto chips = HintChips();
		float width = HintGroupGap() * static_cast<float>(chips.size() - 1);
		for (const auto& chip : chips)
			width += MeasureHint(chip).width;
		return width;
	}

	void DrawHints() noexcept
	{
		if (!IsNavigating())
			return;
		const Theme::FontGuard font{ Theme::FontRole::kSubtext };
		const auto start = ImGui::GetCursorScreenPos();
		auto p = start;
		auto* draw = ImGui::GetWindowDrawList();
		const auto& style = ImGui::GetStyle();
		const auto height = ImGui::GetTextLineHeight();
		const auto groupGap = HintGroupGap();
		for (const auto& chip : HintChips())
		{
			const auto metrics = MeasureHint(chip);
			draw->AddRectFilled(p, { p.x + metrics.keyWidth, p.y + height },
				ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
			draw->AddText({ p.x + style.FramePadding.x, p.y },
				ImGui::GetColorU32(ImGuiCol_Text), chip.first);
			draw->AddText({ p.x + metrics.keyWidth + metrics.labelGap, p.y },
				ImGui::GetColorU32(ImGuiCol_Text, 0.75f), chip.second);
			p.x += metrics.width + groupGap;
		}
		ImGui::Dummy({ p.x - start.x - groupGap, height });
	}
}
