#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/host/Hotkeys.h>

#include <optional>
#include <string>

namespace DearModdingUI
{
	// The host key binding editor: the DMUI_KeyCaptureDescriptor comment defines
	// each field. It edits nothing itself; the caller applies a captured chord.
	struct KeyCaptureSpec
	{
		// Scopes the control and is never drawn.
		const char* id{};
		// A serialized chord; empty or "none" is unbound.
		const char* binding{};
		// Replaces the formatted binding when not empty.
		const char* displayText{};
		const char* details{};
		// Explains a conflict or invalid binding and tints the field.
		const char* warning{};
		HotkeySlot slot{ HotkeySlot::kKeyboardMouse };
		bool allowClear{ true };
		bool readOnly{};
		// Zero uses the available width.
		float width{};
	};

	[[nodiscard]] DMUI_Result ValidateKeyCapture(
		const DMUI_KeyCaptureDescriptor* a_capture) noexcept;
	[[nodiscard]] KeyCaptureSpec MakeKeyCaptureSpec(
		const DMUI_KeyCaptureDescriptor& a_capture) noexcept;

	// What the field reads in the given state.
	[[nodiscard]] std::string KeyCaptureText(
		const KeyCaptureSpec& a_spec,
		bool a_capturing);

	// Draws at the cursor. Returns the chord captured on this frame, or an empty
	// chord when the binding was cleared.
	[[nodiscard]] std::optional<HotkeyChord> DrawKeyCapture(
		const KeyCaptureSpec& a_spec) noexcept;
}
