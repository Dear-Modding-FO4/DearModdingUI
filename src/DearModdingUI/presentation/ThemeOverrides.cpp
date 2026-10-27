#include <DearModdingUI/presentation/ThemeSpec.h>
#include <DearModdingUI/settings/HostSettings.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace DearModdingUI::Theme
{
	ThemeSpec ApplyThemeOverrides(
		ThemeSpec a_theme,
		const ThemeOverrides& a_overrides) noexcept
	{
		auto& colors = a_theme.style.Colors;
		auto& effects = a_theme.effects;
		if (a_overrides.accent)
		{
			RetintAccentColors(a_theme.style, *a_overrides.accent);
			a_theme.tokens.accent.x = a_overrides.accent->x;
			a_theme.tokens.accent.y = a_overrides.accent->y;
			a_theme.tokens.accent.z = a_overrides.accent->z;
			DeriveTokens(a_theme, false, false);
		}
		if (a_overrides.windowOpacity)
		{
			colors[ImGuiCol_WindowBg].w =
				std::clamp(*a_overrides.windowOpacity, 0.0f, 1.0f);
		}
		if (a_overrides.paletteColor)
		{
			colors[ImGuiCol_PopupBg].x = a_overrides.paletteColor->x;
			colors[ImGuiCol_PopupBg].y = a_overrides.paletteColor->y;
			colors[ImGuiCol_PopupBg].z = a_overrides.paletteColor->z;
		}
		if (a_overrides.paletteOpacity)
		{
			colors[ImGuiCol_PopupBg].w =
				std::clamp(*a_overrides.paletteOpacity, 0.0f, 1.0f);
		}
		if (a_overrides.iconColorMode)
		{
			effects.iconColorMode = *a_overrides.iconColorMode;
			DeriveTokens(a_theme, true, false);
		}
		if (a_overrides.backgroundBlur)
			effects.backgroundBlur = *a_overrides.backgroundBlur;
		if (a_overrides.backgroundBlurStrength)
			effects.backgroundBlurStrength = *a_overrides.backgroundBlurStrength;
		if (a_overrides.logoColors)
			effects.logoColorMode = *a_overrides.logoColors;
		if (a_overrides.bodyFontFamily)
		{
			a_theme.typography.roles[static_cast<size_t>(FontRole::kBody)].family =
				*a_overrides.bodyFontFamily;
		}
		if (a_overrides.monospaceFontFamily)
		{
			a_theme.typography.roles[static_cast<size_t>(FontRole::kMonospace)].family =
				*a_overrides.monospaceFontFamily;
		}
		return a_theme;
	}

	ThemeOverrides MakeThemeOverrides(
		const HostAppearanceOverrides& a_appearance) noexcept
	{
		ThemeOverrides overrides;
		if (a_appearance.accentColor)
			overrides.accent = HostAccentToImVec4(*a_appearance.accentColor);
		overrides.windowOpacity = a_appearance.windowBackgroundOpacity;
		if (a_appearance.paletteBackgroundColor)
		{
			overrides.paletteColor =
				HostAccentToImVec4(*a_appearance.paletteBackgroundColor);
		}
		overrides.paletteOpacity = a_appearance.paletteBackgroundOpacity;
		overrides.iconColorMode = a_appearance.iconColorMode;
		overrides.backgroundBlur = a_appearance.backgroundBlur;
		overrides.backgroundBlurStrength = a_appearance.backgroundBlurStrength;
		overrides.logoColors = a_appearance.logoColors;
		overrides.bodyFontFamily = a_appearance.bodyFontFamily;
		overrides.monospaceFontFamily = a_appearance.monospaceFontFamily;
		return overrides;
	}

	HostAppearanceOverrides DescribeAppearance(const ThemeSpec& a_theme) noexcept
	{
		const auto& popup = a_theme.style.Colors[ImGuiCol_PopupBg];
		HostAppearanceOverrides appearance;
		appearance.accentColor = HostAccentFromImVec4(a_theme.tokens.accent);
		appearance.windowBackgroundOpacity =
			a_theme.style.Colors[ImGuiCol_WindowBg].w;
		appearance.paletteBackgroundColor = HostAccentFromImVec4(popup);
		appearance.paletteBackgroundOpacity = popup.w;
		appearance.iconColorMode = a_theme.effects.iconColorMode;
		appearance.backgroundBlur = a_theme.effects.backgroundBlur;
		appearance.backgroundBlurStrength = a_theme.effects.backgroundBlurStrength;
		appearance.logoColors = a_theme.effects.logoColorMode;
		appearance.bodyFontFamily =
			a_theme.typography.roles[static_cast<size_t>(FontRole::kBody)].family;
		appearance.monospaceFontFamily =
			a_theme.typography.roles[static_cast<size_t>(FontRole::kMonospace)].family;
		return appearance;
	}
}
