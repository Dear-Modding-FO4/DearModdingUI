#include <DearModdingUI/presentation/Theme.h>

namespace DearModdingUI::Theme
{
	DMUI_ThemeColors MakeColorSnapshot(const ImVec4& a_accent) noexcept
	{
		const auto convert = [](const ImVec4& a_color) {
			return DMUI_Vec4{ a_color.x, a_color.y, a_color.z, a_color.w };
		};
		auto mutedAccent = a_accent;
		mutedAccent.w = colors::kMutedAccentOpacity;
		return {
			sizeof(DMUI_ThemeColors),
			convert(colors::kSuccess),
			convert(colors::kWarning),
			convert(colors::kError),
			convert(colors::kInfo),
			convert(colors::kMuted),
			convert(a_accent),
			convert(mutedAccent),
			convert(kStatusPaletteDefaults.disable),
			convert(kStatusPaletteDefaults.error),
			convert(kStatusPaletteDefaults.warning),
			convert(kStatusPaletteDefaults.restartNeeded),
			convert(kStatusPaletteDefaults.currentHotkey),
			convert(kStatusPaletteDefaults.success),
			convert(kStatusPaletteDefaults.info)
		};
	}

	ImVec4 TextColor(dmui::TextTone a_tone) noexcept
	{
		const auto snapshot = ColorSnapshot();
		const auto resolved = dmui::ResolveTextColor(snapshot, a_tone);
		if (!resolved || !resolved.color)
			return kFullPalette[ImGuiCol_Text];
		return {
			resolved.color->x,
			resolved.color->y,
			resolved.color->z,
			resolved.color->w
		};
	}

	ImVec4 StatusTextColor(DMUI_StatusSeverity a_severity) noexcept
	{
		switch (a_severity)
		{
		case DMUI_STATUS_SEVERITY_SUCCESS:
			return TextColor(dmui::TextTone::kStatusSuccess);
		case DMUI_STATUS_SEVERITY_WARNING:
			return TextColor(dmui::TextTone::kStatusWarning);
		case DMUI_STATUS_SEVERITY_ERROR:
			return TextColor(dmui::TextTone::kStatusError);
		default:
			return TextColor(dmui::TextTone::kStatusInfo);
		}
	}
}
