#include "HostSettingsColorControls.h"

#include <DearModdingUI/controls/Button.h>
#include <DearModdingUI/localization/Localization.h>

#include <imgui/imgui.h>

namespace DearModdingUI::HostSettingsViewDetail
{
	bool DrawGameColorSyncControls(
		HostAccentColor& a_color,
		std::optional<HostAccentColor> a_hudColor,
		std::optional<HostAccentColor> a_pipboyColor,
		float a_width) noexcept
	{
		const auto drawButton = [&](
			const char* a_id,
			const char* a_label,
			std::optional<HostAccentColor> a_source) noexcept {
			const auto pressed = DrawButton({
				.id = a_id,
				.label = a_label,
				.tooltip = a_source ?
					Localization::Text(
						"$DMUI_Settings_SyncColorAvailable",
						"Copy this game color to the accent preview. Use Apply to save.") :
					Localization::Text(
						"$DMUI_Settings_SyncColorUnavailable",
						"Color unavailable. Requires Fallout 4 with valid game color preferences."),
				.enabled = a_source.has_value()
			});
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
		const auto buttonWidth =
			MeasureButton({ .label = hudText }).x +
			MeasureButton({ .label = pipboyText }).x +
			ImGui::GetStyle().ItemSpacing.x;
		auto changed = drawButton("##SyncFromHud", hudText, a_hudColor);
		if (buttonWidth <= a_width)
			ImGui::SameLine();
		changed |= drawButton("##SyncFromPipboy", pipboyText, a_pipboyColor);
		return changed;
	}
}
