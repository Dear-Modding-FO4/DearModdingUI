#pragma once

#include <DearModdingUI/host/Hotkeys.h>
#include <imgui/imgui.h>
#include <array>
#include <cstdint>
#include <optional>

namespace DearModdingUI::ControllerNavigation
{
	inline constexpr std::array kButtonKeys{
		ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown,
		ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight,
		ImGuiKey_GamepadStart, ImGuiKey_GamepadBack,
		ImGuiKey_GamepadL3, ImGuiKey_GamepadR3,
		ImGuiKey_GamepadL1, ImGuiKey_GamepadR1,
		ImGuiKey_GamepadFaceDown, ImGuiKey_GamepadFaceRight,
		ImGuiKey_GamepadFaceLeft, ImGuiKey_GamepadFaceUp,
		ImGuiKey_GamepadL2, ImGuiKey_GamepadR2
	};

	[[nodiscard]] constexpr ImGuiKey ButtonKey(uint32_t a_code) noexcept
	{
		return a_code >= KeyCatalog::kGamepadButtonOffset &&
			a_code < KeyCatalog::kGamepadButtonOffset + KeyCatalog::kGamepadButtonCount ?
			kButtonKeys[a_code - KeyCatalog::kGamepadButtonOffset] : ImGuiKey_None;
	}

	static_assert(kButtonKeys.size() == KeyCatalog::kGamepadButtonCount &&
		ButtonKey(KeyCatalog::kPadUp) == ImGuiKey_GamepadDpadUp &&
		ButtonKey(KeyCatalog::kPadStart) == ImGuiKey_GamepadStart &&
		ButtonKey(KeyCatalog::kPadLB) == ImGuiKey_GamepadL1 &&
		ButtonKey(KeyCatalog::kPadA) == ImGuiKey_GamepadFaceDown &&
		ButtonKey(KeyCatalog::kPadB) == ImGuiKey_GamepadFaceRight &&
		ButtonKey(KeyCatalog::kPadRT) == ImGuiKey_GamepadR2);

	enum class ControllerMode { kCursor, kNavigation };
	enum class ControllerAction { kNative, kCursorClick, kBack, kClose, kSearch, kReset, kSidebar, kContent, kToggleMode };

	[[nodiscard]] constexpr ControllerMode DecideControllerMode(
		ControllerMode a_mode, bool a_cursorMoved, uint32_t a_button) noexcept
	{
		if (a_cursorMoved)
			return ControllerMode::kCursor;
		if (a_button == KeyCatalog::kPadLS)
			return a_mode == ControllerMode::kNavigation ? ControllerMode::kCursor : ControllerMode::kNavigation;
		if (a_button >= KeyCatalog::kPadUp && a_button <= KeyCatalog::kPadRight)
			return ControllerMode::kNavigation;
		return a_mode;
	}

	[[nodiscard]] constexpr ControllerAction DecideControllerAction(
		uint32_t a_button, ControllerMode a_mode, bool a_editing,
		bool a_hasResetTarget) noexcept
	{
		if (a_button == KeyCatalog::kPadLS)
			return ControllerAction::kToggleMode;
		if (a_button == KeyCatalog::kPadB)
			return ControllerAction::kBack;
		if (a_button == KeyCatalog::kPadStart)
			return ControllerAction::kClose;
		if (a_button == KeyCatalog::kPadA && a_mode == ControllerMode::kCursor)
			return ControllerAction::kCursorClick;
		if (a_editing)
			return ControllerAction::kNative;
		switch (a_button)
		{
		case KeyCatalog::kPadLB: return ControllerAction::kSidebar;
		case KeyCatalog::kPadRB: return ControllerAction::kContent;
		case KeyCatalog::kPadY: return ControllerAction::kSearch;
		case KeyCatalog::kPadX: return a_hasResetTarget ? ControllerAction::kReset : ControllerAction::kNative;
		default: return ControllerAction::kNative;
		}
	}

	[[nodiscard]] constexpr float AnalogValue(float a_value, float a_deadzone = 0.1f) noexcept
	{
		return a_value <= a_deadzone ? 0.0f :
			a_value >= 1.0f ? 1.0f : (a_value - a_deadzone) / (1.0f - a_deadzone);
	}

	enum class Pane { kSidebar, kContent };
	[[nodiscard]] HotkeyMessageResult RouteButton(
		uint32_t a_code, bool a_pressed, bool a_repeat, float a_value,
		HotkeyContextState a_context) noexcept;
	void QueueButton(uint32_t a_code, float a_value) noexcept;
	void QueueStick(bool a_left, float a_x, float a_y) noexcept;
	void ObserveCursor(float a_x, float a_y) noexcept;
	void UseCursor() noexcept;
	void UseNavigation() noexcept;
	[[nodiscard]] bool IsNavigating() noexcept;
	[[nodiscard]] std::optional<ImVec2> TakeCursorWarp() noexcept;
	void Reset() noexcept;
	void BeginDesktopInput() noexcept;
	bool PrepareFrame(bool a_visible, bool a_desktop = false) noexcept;
	void BeginShell() noexcept;
	void BeginPane(Pane a_pane) noexcept;
	void EndPanes() noexcept;
	void FocusSelectedItem(bool a_selected) noexcept;
	[[nodiscard]] bool TakeSearch() noexcept;
	[[nodiscard]] bool TakeClose() noexcept;
	[[nodiscard]] bool ResetFocusedRow(bool a_available, const ImVec2& a_min, const ImVec2& a_max) noexcept;
	void DrawHints() noexcept;
	[[nodiscard]] float HintsWidth() noexcept;
}
