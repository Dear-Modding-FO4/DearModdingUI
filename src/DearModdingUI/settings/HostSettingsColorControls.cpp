#include "HostSettingsColorControls.h"

#include <DearModdingUI/localization/Localization.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <format>
#include <string>

namespace DearModdingUI::HostSettingsViewDetail
{
	bool DrawGameColorSyncControls(
		HostAccentColor& a_color,
		std::optional<HostAccentColor> a_hudColor,
		std::optional<HostAccentColor> a_pipboyColor,
		float a_width) noexcept
	{
		const auto drawButton = [&](
			const std::string& a_label,
			std::optional<HostAccentColor> a_source) noexcept {
			ImGui::BeginDisabled(!a_source);
			const auto pressed = ImGui::Button(a_label.c_str());
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", a_source ?
					Localization::Text(
						"$DMUI_Settings_SyncColorAvailable",
						"Copy this game color to the accent preview. Use Apply to save.") :
					Localization::Text(
						"$DMUI_Settings_SyncColorUnavailable",
						"Color unavailable. Requires Fallout 4 with valid game color preferences."));
			}
			if (!pressed || !a_source || a_color == *a_source)
				return false;
			a_color = *a_source;
			return true;
		};

		const auto* hudText = Localization::Text(
			"$DMUI_Settings_SyncFromHud",
			"Sync from HUD");
		const auto* pipboyText = Localization::Text(
			"$DMUI_Settings_SyncFromPipboy",
			"Sync from Pip-Boy");
		const auto hudLabel = std::format("{}###SyncFromHud", hudText);
		const auto pipboyLabel = std::format("{}###SyncFromPipboy", pipboyText);
		const auto& style = ImGui::GetStyle();
		const auto buttonWidth = ImGui::CalcTextSize(hudText).x +
			ImGui::CalcTextSize(pipboyText).x +
			style.FramePadding.x * 4.0f + style.ItemSpacing.x;
		auto changed = drawButton(hudLabel, a_hudColor);
		if (buttonWidth <= a_width)
			ImGui::SameLine();
		changed |= drawButton(pipboyLabel, a_pipboyColor);
		return changed;
	}

	bool DrawColorSettingControl(
		HostAccentColor& a_color,
		std::span<const ColorPreset> a_presets,
		float a_width) noexcept
	{
		auto changed = false;
		a_width = (std::max)(a_width, 1.0f);
		const auto origin = ImGui::GetCursorScreenPos();
		const auto targetRight = origin.x + a_width;

		auto color = HostAccentToImVec4(a_color);
		ImGui::SetNextItemWidth(a_width);
		if (ImGui::ColorEdit3(
				"##Value",
				&color.x,
				ImGuiColorEditFlags_NoAlpha |
					ImGuiColorEditFlags_DisplayRGB |
					ImGuiColorEditFlags_InputRGB |
					ImGuiColorEditFlags_PickerHueBar))
		{
			a_color = HostAccentFromImVec4(color);
			changed = true;
		}

		if (a_presets.empty())
			return changed;

		const auto* label = Localization::Text(
			"$DMUI_Settings_ColorVisionPresets",
			"Color-vision-friendly presets");
		const auto& style = ImGui::GetStyle();
		const auto swatchHeight = ImGui::GetFrameHeight();
		const auto labelWidth = ImGui::CalcTextSize(label).x;
		const auto minimumSwatchesWidth =
			swatchHeight * static_cast<float>(a_presets.size()) +
			style.ItemSpacing.x *
				static_cast<float>(a_presets.size() - 1);
		const auto presetsInline =
			labelWidth + style.ItemSpacing.x + minimumSwatchesWidth <=
				a_width;

		ImGui::AlignTextToFramePadding();
		ImGui::PushTextWrapPos(
			ImGui::GetCursorPosX() + a_width);
		ImGui::TextUnformatted(label);
		ImGui::PopTextWrapPos();
		if (presetsInline)
			ImGui::SameLine();

		auto swatchOrigin = ImGui::GetCursorScreenPos();
		const auto swatchSpan = (std::max)(
			targetRight - swatchOrigin.x,
			1.0f);
		const auto count = static_cast<float>(a_presets.size());
		auto gap = style.ItemSpacing.x;
		if (a_presets.size() > 1)
		{
			gap = (std::min)(
				gap,
				(std::max)(
					(swatchSpan - count) /
						static_cast<float>(a_presets.size() - 1),
					0.0f));
		}
		else
			gap = 0.0f;

		for (size_t index = 0; index < a_presets.size(); ++index)
		{
			if (index > 0)
				ImGui::SameLine(0.0f, gap);
			const auto remainingCount =
				static_cast<float>(a_presets.size() - index);
			const auto buttonOrigin = ImGui::GetCursorScreenPos();
			const auto remainingWidth = (std::max)(
				targetRight - buttonOrigin.x,
				1.0f);
			const auto swatchWidth = index + 1 == a_presets.size() ?
				remainingWidth :
				(std::max)(
					(remainingWidth -
						gap * (remainingCount - 1.0f)) /
						remainingCount,
					1.0f);

			const auto& preset = a_presets[index];
			ImGui::PushID(static_cast<int>(index));
			const auto presetId = std::format("{}###Preset", preset.name);
			if (ImGui::ColorButton(
					presetId.c_str(),
					HostAccentToImVec4(preset.color),
					ImGuiColorEditFlags_NoAlpha,
					{ swatchWidth, swatchHeight }))
			{
				a_color = preset.color;
				changed = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", preset.description);
			ImGui::PopID();
		}
		return changed;
	}
}
