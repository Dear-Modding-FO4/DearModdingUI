#pragma once

#include <DearModdingUI/presentation/FontCatalog.h>
#include <DearModdingUI/presentation/FontMetrics.h>
#include <DearModdingUI/presentation/IconSet.h>

#include <imgui/imgui.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace DearModdingUI
{
	struct HostAppearanceOverrides;

	enum class LogoColorMode
	{
		kOriginal,
		kAccent
	};
}

namespace DearModdingUI::Theme
{
	enum class IconColorMode : uint32_t
	{
		kColored,
		kMonochrome
	};

	struct Tokens
	{
		ImVec4 text;
		ImVec4 muted;
		ImVec4 accent;
		ImVec4 accentMuted;
		ImVec4 info;
		ImVec4 success;
		ImVec4 warning;
		ImVec4 error;
		ImVec4 disabled;
		ImVec4 restartNeeded;
		ImVec4 currentHotkey;
		ImVec4 heading;
		ImVec4 headingHovered;
		ImVec4 searchFrameActive;
		ImVec4 rowAlt;
		ImVec4 iconTint;
		ImVec4 logoTop;
		ImVec4 logoLeft;
		ImVec4 logoRight;
		ImVec4 logoBottom;
	};

	struct Chrome
	{
		float headerFallbackTextScale;
		float separatorThickness;
		float featureTitleScale;
		float versionTextOpacity;
		float headingMinimizedFactor;
		float searchIconSize;
		float searchIconAlpha;
		float searchIconOffsetX;
		float searchInputPaddingExtra;
		float searchInputFramePaddingY;
		float searchIconStrokeRatio;
		float mutedAccentOpacity;
		// Multiplies the body font size.
		float sectionGapScale;
		// Unscaled; ApplyStyle scales it with the style.
		ImVec2 panelPadding;
	};

	struct Effects
	{
		bool backgroundBlur;
		float backgroundBlurStrength;
		IconColorMode iconColorMode;
		LogoColorMode logoColorMode;
	};

	// Selects the host's control renderer; new looks add enumerators.
	enum class ControlRendererKind : uint32_t
	{
		kDefault
	};

	struct RoleTypography
	{
		// A bundled family name, or FontCatalog::kBuiltinFamily.
		std::string family;
		FontCatalog::FontWeight weight;
		bool italic;
		// Multiplies the base font size.
		float sizeScale;

		[[nodiscard]] bool operator==(const RoleTypography&) const noexcept = default;
	};

	struct Typography
	{
		// Body pixels per back-buffer pixel of height.
		float baseFontRatio;
		std::array<RoleTypography, kFontRoleCount> roles;

		[[nodiscard]] bool operator==(const Typography&) const noexcept = default;
	};

	// Every host visual decision; style is unscaled and ApplyStyle scales it.
	struct ThemeSpec
	{
		ImGuiStyle style;
		Tokens tokens;
		Chrome chrome;
		Effects effects;
		Typography typography;
		IconSet icons;
		ControlRendererKind controlRenderer{ ControlRendererKind::kDefault };
	};

	// User choices layered over the selected preset; unset keeps the preset's.
	struct ThemeOverrides
	{
		// Only the color channels apply.
		std::optional<ImVec4> accent;
		std::optional<float> windowOpacity;
		// Only the color channels apply; opacity is separate.
		std::optional<ImVec4> paletteColor;
		std::optional<float> paletteOpacity;
		std::optional<IconColorMode> iconColorMode;
		std::optional<bool> backgroundBlur;
		std::optional<float> backgroundBlurStrength;
		std::optional<LogoColorMode> logoColors;
		std::optional<std::string> bodyFontFamily;
		std::optional<std::string> monospaceFontFamily;
	};

	// The compiled base every preset file layers over.
	[[nodiscard]] ThemeSpec MakeDefaultTheme(const ImVec4& a_accent) noexcept;
	[[nodiscard]] ThemeSpec MakeDefaultTheme() noexcept;
	// Sets the accent-driven slots' hue and keeps each slot's alpha.
	void RetintAccentColors(ImGuiStyle& a_style, const ImVec4& a_accent) noexcept;
	// Re-derives the tokens that follow accent, icon mode, and text color.
	void DeriveTokens(
		ThemeSpec& a_theme,
		bool a_keepAccentMuted,
		bool a_keepIconTint) noexcept;
	[[nodiscard]] ThemeSpec ApplyThemeOverrides(
		ThemeSpec a_theme,
		const ThemeOverrides& a_overrides) noexcept;
	[[nodiscard]] ThemeOverrides MakeThemeOverrides(
		const HostAppearanceOverrides& a_appearance) noexcept;
	// Every overridable value as the theme itself resolves it.
	[[nodiscard]] HostAppearanceOverrides DescribeAppearance(
		const ThemeSpec& a_theme) noexcept;
	// Valid on the calling thread until its next call.
	[[nodiscard]] const ThemeSpec& Current() noexcept;
}
