#include <DearModdingUI/controls/Button.h>

#include <DearModdingUI/controls/ControlRenderer.h>
#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/controls/Tooltip.h>

#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace DearModdingUI
{
	namespace
	{
		[[nodiscard]] const char* ItemId(const ButtonSpec& a_button) noexcept
		{
			return a_button.id && a_button.id[0] != '\0' ?
				a_button.id :
				a_button.label;
		}

		[[nodiscard]] ButtonContent ContentOf(
			const ButtonSpec& a_button) noexcept
		{
			return { a_button.label, a_button.glyph, a_button.glyphSize };
		}
	}

	DMUI_Result ValidateButton(const DMUI_ButtonDescriptor* a_button) noexcept
	{
		if (!a_button || a_button->reserved != 0 ||
			a_button->style > DMUI_BUTTON_STYLE_SUBTLE ||
			!std::isfinite(a_button->size.x) ||
			!std::isfinite(a_button->size.y))
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto hasId = a_button->id && a_button->id[0] != '\0';
		const auto hasLabel = a_button->label && a_button->label[0] != '\0';
		return hasId || hasLabel ?
			DMUI_RESULT_OK :
			DMUI_RESULT_INVALID_ARGUMENT;
	}

	ButtonSpec MakeButtonSpec(const DMUI_ButtonDescriptor& a_button) noexcept
	{
		return {
			a_button.id,
			a_button.label,
			static_cast<char32_t>(a_button.glyph),
			a_button.tooltip,
			a_button.enabled != 0,
			static_cast<dmui::ButtonStyle>(a_button.style),
			{ a_button.size.x, a_button.size.y }
		};
	}

	ButtonSpec MakeIconButtonSpec(
		const char* a_id,
		char32_t a_glyph,
		const char* a_fallbackLabel,
		const char* a_tooltip) noexcept
	{
		return {
			.id = a_id,
			.label = HasIconGlyph(a_glyph) ? nullptr : a_fallbackLabel,
			.glyph = a_glyph,
			.tooltip = a_tooltip,
			.style = dmui::ButtonStyle::kSubtle
		};
	}

	ImVec2 MeasureButton(const ButtonSpec& a_button) noexcept
	{
		const auto natural = CurrentControlRenderer().MeasureButton(
			ContentOf(a_button),
			a_button.style);
		const auto size = ImGui::CalcItemSize(
			a_button.size,
			natural.x,
			natural.y);
		return { (std::max)(size.x, 1.0f), (std::max)(size.y, 1.0f) };
	}

	bool DrawButton(const ButtonSpec& a_button) noexcept
	{
		const auto* window = ImGui::GetCurrentWindow();
		const auto* id = ItemId(a_button);
		if (!window || window->SkipItems || !id)
			return false;
		const auto size = MeasureButton(a_button);

		ImGui::PushID(id);
		ImGui::BeginDisabled(!a_button.enabled);
		const auto pressed = ImGui::InvisibleButton(
			"##Button",
			size,
			ImGuiButtonFlags_EnableNav);
		CurrentControlRenderer().DrawButton(
			{ ImGui::GetItemRectMin(), ImGui::GetItemRectMax() },
			ContentOf(a_button),
			a_button.style,
			{
				ImGui::GetItemID(),
				ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled),
				ImGui::IsItemActive(),
				ImGui::IsItemFocused(),
				a_button.active
			});
		ImGui::EndDisabled();
		ImGui::PopID();
		if (a_button.tooltip)
			Tooltip::Item(a_button.tooltip);
		return pressed && a_button.enabled;
	}

	bool DrawButtonAt(const ButtonSpec& a_button, const ImVec2& a_origin) noexcept
	{
		if (!ImGui::GetCurrentWindow())
			return false;
		const auto restore = ImGui::GetCursorScreenPos();
		ImGui::SetCursorScreenPos(a_origin);
		const auto pressed = DrawButton(a_button);
		ImGui::SetCursorScreenPos(restore);
		ImGui::Dummy({ 0.0f, 0.0f });
		return pressed;
	}
}
