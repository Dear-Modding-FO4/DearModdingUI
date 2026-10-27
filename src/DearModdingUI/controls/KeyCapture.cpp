#include <DearModdingUI/controls/KeyCapture.h>

#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/controls/Button.h>
#include <DearModdingUI/controls/ControlRenderer.h>
#include <DearModdingUI/localization/Localization.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr DMUI_KeyCaptureFlags kKnownFlags{
			DMUI_KEY_CAPTURE_ALLOW_CLEAR | DMUI_KEY_CAPTURE_READ_ONLY
		};

		[[nodiscard]] bool IsBound(const char* a_binding) noexcept
		{
			return a_binding && a_binding[0] != '\0' &&
				!EqualsIgnoringCase(a_binding, "none");
		}

		[[nodiscard]] bool Present(const char* a_text) noexcept
		{
			return a_text && a_text[0] != '\0';
		}
	}

	DMUI_Result ValidateKeyCapture(const DMUI_KeyCaptureDescriptor* a_capture) noexcept
	{
		if (!a_capture || !a_capture->id || a_capture->id[0] == '\0' ||
			a_capture->reserved != 0 ||
			(a_capture->flags & ~kKnownFlags) != 0 ||
			a_capture->slot > DMUI_KEY_CAPTURE_SLOT_GAMEPAD ||
			!std::isfinite(a_capture->width) || a_capture->width < 0.0f)
			return DMUI_RESULT_INVALID_ARGUMENT;
		return DMUI_RESULT_OK;
	}

	KeyCaptureSpec MakeKeyCaptureSpec(const DMUI_KeyCaptureDescriptor& a_capture) noexcept
	{
		return {
			a_capture.id,
			a_capture.binding,
			a_capture.displayText,
			a_capture.details,
			a_capture.warning,
			a_capture.slot == DMUI_KEY_CAPTURE_SLOT_GAMEPAD ?
				HotkeySlot::kGamepad :
				HotkeySlot::kKeyboardMouse,
			(a_capture.flags & DMUI_KEY_CAPTURE_ALLOW_CLEAR) != 0,
			(a_capture.flags & DMUI_KEY_CAPTURE_READ_ONLY) != 0,
			a_capture.width
		};
	}

	std::string KeyCaptureText(const KeyCaptureSpec& a_spec, bool a_capturing)
	{
		if (a_capturing)
		{
			return a_spec.slot == HotkeySlot::kGamepad ?
				Localization::Text("$DMUI_Controls_PressButtons", "Press buttons...") :
				Localization::Text("$DMUI_Controls_PressKeys", "Press keys...");
		}
		if (Present(a_spec.displayText))
			return a_spec.displayText;
		if (IsBound(a_spec.binding))
			return FormatHotkeyChord(a_spec.binding);
		return Present(a_spec.warning) ?
			Localization::Text("$DMUI_Controls_CheckBinding", "Check binding") :
			HotkeyNotSetText();
	}

	std::optional<HotkeyChord> DrawKeyCapture(const KeyCaptureSpec& a_spec) noexcept
	{
		// One capture runs at a time, owned by the control that started it.
		static ImGuiID captureTarget{};
		const auto id = ImGui::GetID(a_spec.id);
		std::optional<HotkeyChord> captured;
		if (captureTarget == id)
		{
			captured = Hotkeys::TakeCapture();
			if (captured || !Hotkeys::IsCapturing())
				captureTarget = 0;
		}

		const auto& renderer = CurrentControlRenderer();
		const auto capturing = captureTarget == id;
		const auto bound = IsBound(a_spec.binding);
		const auto warned = Present(a_spec.warning) && !capturing;
		const auto text = KeyCaptureText(a_spec, capturing);
		const auto width = a_spec.width > 0.0f ?
			a_spec.width :
			ImGui::GetContentRegionAvail().x;
		const auto height = ImGui::GetFrameHeight();
		const auto clearWidth = height;
		const auto origin = ImGui::GetCursorScreenPos();

		std::string tooltip;
		const auto addLine = [&](const char* a_line) {
			if (!Present(a_line))
				return;
			if (!tooltip.empty())
				tooltip += '\n';
			tooltip += a_line;
		};
		addLine(a_spec.details);
		addLine(text.c_str());
		addLine(a_spec.warning);
		if (!a_spec.readOnly)
		{
			addLine(Localization::Text(
				"$DMUI_Controls_ReleaseToBind",
				"Release to bind. Esc / B cancels."));
		}

		ImGui::PushID(a_spec.id);
		const auto frame = renderer.PushKeyCaptureStyle(
			{ bound || capturing, capturing, warned });
		const auto pressed = DrawButton({
			.id = "##Capture",
			.label = text.c_str(),
			.tooltip = tooltip.c_str(),
			.enabled = !a_spec.readOnly,
			.size = { (std::max)(width - clearWidth, 1.0f), height }
		});
		PopStyle(frame);
		if (pressed)
		{
			Hotkeys::BeginCapture(a_spec.slot);
			captureTarget = id;
		}
		if (capturing)
			renderer.DrawKeyCaptureCue({ origin, ImGui::GetItemRectMax() });

		if (bound && a_spec.allowClear && !a_spec.readOnly && !capturing)
		{
			ImGui::SameLine(0.0f, 0.0f);
			auto clear = MakeIconButtonSpec(
				"##Clear",
				dmui::PhosphorGlyph::kX,
				"x",
				Localization::Text("$DMUI_Controls_Unbind", "Unbind"));
			clear.size = { clearWidth, height };
			const auto muted = renderer.PushKeyClearStyle();
			const auto cleared = DrawButtonAt(
				clear,
				{ origin.x + width - clearWidth, origin.y });
			PopStyle(muted);
			if (cleared)
			{
				(void)Hotkeys::CancelCapture();
				captureTarget = 0;
				captured = HotkeyChord{};
			}
		}
		ImGui::SetCursorScreenPos(origin);
		ImGui::Dummy({ width, height });
		ImGui::PopID();
		return captured;
	}
}
