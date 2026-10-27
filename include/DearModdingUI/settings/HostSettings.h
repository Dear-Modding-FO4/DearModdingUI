#pragma once

#include <DearModdingUI/host/MenuToggleChord.h>
#include <DearModdingUI/controls/FieldFeedback.h>
#include <DearModdingUI/navigation/Sidebar.h>
#include <DearModdingUI/presentation/FontMetrics.h>
#include <DearModdingUI/presentation/ThemeSpec.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace DearModdingUI
{
	[[nodiscard]] constexpr std::optional<LogoColorMode> ParseLogoColorMode(
		std::string_view a_value) noexcept
	{
		if (a_value == "original")
			return LogoColorMode::kOriginal;
		if (a_value == "accent")
			return LogoColorMode::kAccent;
		return std::nullopt;
	}

	[[nodiscard]] constexpr std::string_view LogoColorModeName(
		LogoColorMode a_mode) noexcept
	{
		return a_mode == LogoColorMode::kAccent ? "accent" : "original";
	}

	struct HostAccentColor
	{
		uint8_t red{ 0x42 };
		uint8_t green{ 0xFA };
		uint8_t blue{ 0x60 };

		[[nodiscard]] constexpr bool operator==(
			const HostAccentColor&) const noexcept = default;
	};

	using HostPaletteColor = HostAccentColor;

	inline constexpr HostAccentColor kDefaultHostAccentColor{};
	inline constexpr HostPaletteColor kDefaultPaletteBackgroundColor{
		0x05, 0x05, 0x05
	};
	inline constexpr float kDefaultWindowBackgroundOpacity{ 0.75f };
	inline constexpr float kMinWindowBackgroundOpacity{ 0.20f };
	inline constexpr float kMaxWindowBackgroundOpacity{ 1.0f };
	inline constexpr float kDefaultPaletteBackgroundOpacity{ 0.85f };
	inline constexpr float kMinPaletteBackgroundOpacity{ 0.20f };
	inline constexpr float kMaxPaletteBackgroundOpacity{ 1.0f };
	inline constexpr float kDefaultBackgroundBlurStrength{ 0.50f };
	inline constexpr float kMinBackgroundBlurStrength{ 0.10f };
	inline constexpr float kMaxBackgroundBlurStrength{ 1.0f };
	inline constexpr std::string_view kDefaultBodyFontFamily{ "Jost" };
	inline constexpr std::string_view kDefaultMonospaceFontFamily{ "JetBrains Mono" };

	inline constexpr std::string_view kDefaultThemePreset{ "Default" };

	// Each set value replaces the selected theme preset's own.
	struct HostAppearanceOverrides
	{
		std::optional<HostAccentColor> accentColor;
		std::optional<float> windowBackgroundOpacity;
		std::optional<HostPaletteColor> paletteBackgroundColor;
		std::optional<float> paletteBackgroundOpacity;
		std::optional<Theme::IconColorMode> iconColorMode;
		std::optional<bool> backgroundBlur;
		std::optional<float> backgroundBlurStrength;
		std::optional<LogoColorMode> logoColors;
		std::optional<std::string> bodyFontFamily;
		std::optional<std::string> monospaceFontFamily;

		[[nodiscard]] bool operator==(
			const HostAppearanceOverrides&) const noexcept = default;
	};

	struct HostInterfaceSettings
	{
		SidebarLayoutKind sidebarLayout{ DEFAULT_SIDEBAR_LAYOUT };
		std::string themePreset{ kDefaultThemePreset };
		HostAppearanceOverrides appearance;
		float uiScale{ Theme::kDefaultUserScale };
		std::string menuToggleKey{ SerializeHotkeyChord(kMenuDefaultToggleChord) };
		std::string menuToggleGamepad{ SerializeHotkeyChord(kMenuDefaultGamepadToggleChord) };
		FieldFeedbackPlacement feedbackPlacement{
			DEFAULT_FIELD_FEEDBACK_LAYOUT
		};
		bool fallSoulsMode{ false };

		[[nodiscard]] bool operator==(
			const HostInterfaceSettings&) const noexcept = default;
	};

	// Font families are committed, so previews never carry them.
	struct HostInterfacePreviewSettings
	{
		SidebarLayoutKind sidebarLayout{ DEFAULT_SIDEBAR_LAYOUT };
		std::string themePreset{ kDefaultThemePreset };
		HostAppearanceOverrides appearance;
		FieldFeedbackPlacement feedbackPlacement{
			DEFAULT_FIELD_FEEDBACK_LAYOUT
		};

		[[nodiscard]] bool operator==(
			const HostInterfacePreviewSettings&) const noexcept = default;
	};

	// An empty text or absent value means the theme preset decides.
	struct PersistedHostInterfaceSettings
	{
		std::optional<bool> monochromeIcons;
		std::string sidebarLayout{
			SidebarLayoutKindName(DEFAULT_SIDEBAR_LAYOUT)
		};
		std::string themePreset{ kDefaultThemePreset };
		std::string accentColor;
		std::optional<float> windowBackgroundOpacity;
		std::string paletteBackgroundColor;
		std::optional<float> paletteBackgroundOpacity;
		std::optional<bool> backgroundBlur;
		std::optional<float> backgroundBlurStrength;
		float uiScale{ Theme::kDefaultUserScale };
		std::string bodyFontFamily;
		std::string monospaceFontFamily;
		std::string menuToggleKey{ SerializeHotkeyChord(kMenuDefaultToggleChord) };
		std::string menuToggleGamepad{ SerializeHotkeyChord(kMenuDefaultGamepadToggleChord) };
		std::map<std::string, std::string> hotkeys;
		std::map<std::string, std::string> gamepadHotkeys;
		std::string feedbackPlacement{
			FieldFeedbackLayoutName(DEFAULT_FIELD_FEEDBACK_LAYOUT)
		};
		bool fallSoulsMode{ false };
		std::string logoColors;

		[[nodiscard]] bool operator==(
			const PersistedHostInterfaceSettings&) const noexcept = default;
	};

	[[nodiscard]] inline float ClampHostSetting(
		float a_value,
		float a_minimum,
		float a_maximum,
		float a_default) noexcept
	{
		return std::isfinite(a_value) ?
			std::clamp(a_value, a_minimum, a_maximum) :
			a_default;
	}

	[[nodiscard]] constexpr int HexDigitValue(char a_value) noexcept
	{
		if (a_value >= '0' && a_value <= '9')
			return a_value - '0';
		if (a_value >= 'a' && a_value <= 'f')
			return a_value - 'a' + 10;
		if (a_value >= 'A' && a_value <= 'F')
			return a_value - 'A' + 10;
		return -1;
	}

	[[nodiscard]] constexpr std::optional<HostAccentColor> TryDecodeHostColor(
		std::string_view a_value) noexcept
	{
		if (a_value.size() == 7 && a_value.front() == '#')
			a_value.remove_prefix(1);
		if (a_value.size() != 6)
			return std::nullopt;

		const auto component = [a_value](size_t a_offset) {
			const auto high = HexDigitValue(a_value[a_offset]);
			const auto low = HexDigitValue(a_value[a_offset + 1]);
			return high < 0 || low < 0 ? -1 : high * 16 + low;
		};
		const auto red = component(0);
		const auto green = component(2);
		const auto blue = component(4);
		if (red < 0 || green < 0 || blue < 0)
			return std::nullopt;
		return HostAccentColor{
			static_cast<uint8_t>(red),
			static_cast<uint8_t>(green),
			static_cast<uint8_t>(blue)
		};
	}

	[[nodiscard]] constexpr HostAccentColor DecodeHostColor(
		std::string_view a_value,
		HostAccentColor a_fallback) noexcept
	{
		const auto decoded = TryDecodeHostColor(a_value);
		return decoded ? *decoded : a_fallback;
	}

	[[nodiscard]] constexpr HostAccentColor DecodeHostAccentColor(
		std::string_view a_value) noexcept
	{
		return DecodeHostColor(a_value, kDefaultHostAccentColor);
	}

	[[nodiscard]] inline std::string EncodeHostAccentColor(
		HostAccentColor a_color)
	{
		constexpr std::string_view digits{ "0123456789ABCDEF" };
		std::string result(7, '#');
		result[1] = digits[a_color.red >> 4];
		result[2] = digits[a_color.red & 0x0F];
		result[3] = digits[a_color.green >> 4];
		result[4] = digits[a_color.green & 0x0F];
		result[5] = digits[a_color.blue >> 4];
		result[6] = digits[a_color.blue & 0x0F];
		return result;
	}

	[[nodiscard]] constexpr ImVec4 HostAccentToImVec4(
		HostAccentColor a_color) noexcept
	{
		constexpr auto inverseByte = 1.0f / 255.0f;
		return {
			static_cast<float>(a_color.red) * inverseByte,
			static_cast<float>(a_color.green) * inverseByte,
			static_cast<float>(a_color.blue) * inverseByte,
			1.0f
		};
	}

	[[nodiscard]] inline HostAccentColor HostAccentFromImVec4(
		const ImVec4& a_color) noexcept
	{
		const auto component = [](float a_value) {
			const auto normalized = std::isfinite(a_value) ?
				std::clamp(a_value, 0.0f, 1.0f) :
				0.0f;
			return static_cast<uint8_t>(std::lround(normalized * 255.0f));
		};
		return {
			component(a_color.x),
			component(a_color.y),
			component(a_color.z)
		};
	}

	// Safe as a single file or folder name under a host data directory.
	[[nodiscard]] constexpr bool IsPlainFileName(
		std::string_view a_value) noexcept
	{
		if (a_value.empty() || a_value.size() > 128 ||
			a_value == "." || a_value == "..")
			return false;
		for (const auto character : a_value)
		{
			const auto byte = static_cast<unsigned char>(character);
			if (byte < 0x20 || character == '/' || character == '\\' ||
				character == ':' || character == '*' || character == '?' ||
				character == '"' || character == '<' || character == '>' ||
				character == '|')
				return false;
		}
		return true;
	}

	// A non-finite value counts as absent so the preset decides.
	inline void NormalizeOverrideRange(
		std::optional<float>& a_value,
		float a_minimum,
		float a_maximum) noexcept
	{
		if (!a_value)
			return;
		if (std::isfinite(*a_value))
			a_value = std::clamp(*a_value, a_minimum, a_maximum);
		else
			a_value.reset();
	}

	[[nodiscard]] inline HostInterfaceSettings NormalizeHostInterfaceSettings(
		HostInterfaceSettings a_settings)
	{
		a_settings.sidebarLayout =
			NormalizeUserSidebarLayout(a_settings.sidebarLayout);
		a_settings.feedbackPlacement =
			NormalizeFieldFeedbackLayout(a_settings.feedbackPlacement);
		if (!IsPlainFileName(a_settings.themePreset))
			a_settings.themePreset = std::string{ kDefaultThemePreset };
		auto& appearance = a_settings.appearance;
		NormalizeOverrideRange(
			appearance.windowBackgroundOpacity,
			kMinWindowBackgroundOpacity,
			kMaxWindowBackgroundOpacity);
		NormalizeOverrideRange(
			appearance.paletteBackgroundOpacity,
			kMinPaletteBackgroundOpacity,
			kMaxPaletteBackgroundOpacity);
		NormalizeOverrideRange(
			appearance.backgroundBlurStrength,
			kMinBackgroundBlurStrength,
			kMaxBackgroundBlurStrength);
		if (appearance.bodyFontFamily &&
			!IsPlainFileName(*appearance.bodyFontFamily))
			appearance.bodyFontFamily.reset();
		if (appearance.monospaceFontFamily &&
			!IsPlainFileName(*appearance.monospaceFontFamily))
			appearance.monospaceFontFamily.reset();
		a_settings.uiScale = ClampHostSetting(
			a_settings.uiScale,
			Theme::kMinUserScale,
			Theme::kMaxUserScale,
			Theme::kDefaultUserScale);
		a_settings.menuToggleKey = SerializeHotkeyChord(
			ParseMenuToggleChord(a_settings.menuToggleKey).chord);
		a_settings.menuToggleGamepad = SerializeHotkeyChord(
			ParseMenuToggleChord(a_settings.menuToggleGamepad, HotkeySlot::kGamepad).chord);
		return a_settings;
	}

	[[nodiscard]] inline std::optional<HostAccentColor> DecodeOptionalHostColor(
		std::string_view a_value) noexcept
	{
		return a_value.empty() ?
			std::nullopt :
			TryDecodeHostColor(a_value);
	}

	[[nodiscard]] inline HostInterfaceSettings DecodeHostInterfaceSettings(
		const PersistedHostInterfaceSettings& a_settings)
	{
		HostAppearanceOverrides appearance;
		appearance.accentColor = DecodeOptionalHostColor(a_settings.accentColor);
		appearance.windowBackgroundOpacity = a_settings.windowBackgroundOpacity;
		appearance.paletteBackgroundColor =
			DecodeOptionalHostColor(a_settings.paletteBackgroundColor);
		appearance.paletteBackgroundOpacity = a_settings.paletteBackgroundOpacity;
		if (a_settings.monochromeIcons)
		{
			appearance.iconColorMode = *a_settings.monochromeIcons ?
				Theme::IconColorMode::kMonochrome :
				Theme::IconColorMode::kColored;
		}
		appearance.backgroundBlur = a_settings.backgroundBlur;
		appearance.backgroundBlurStrength = a_settings.backgroundBlurStrength;
		appearance.logoColors = ParseLogoColorMode(a_settings.logoColors);
		if (!a_settings.bodyFontFamily.empty())
			appearance.bodyFontFamily = a_settings.bodyFontFamily;
		if (!a_settings.monospaceFontFamily.empty())
			appearance.monospaceFontFamily = a_settings.monospaceFontFamily;
		return NormalizeHostInterfaceSettings({
			DecodeUserSidebarLayout(a_settings.sidebarLayout),
			a_settings.themePreset,
			std::move(appearance),
			a_settings.uiScale,
			a_settings.menuToggleKey,
			a_settings.menuToggleGamepad,
			ParseFieldFeedbackLayout(
				a_settings.feedbackPlacement).value_or(
					DEFAULT_FIELD_FEEDBACK_LAYOUT),
			a_settings.fallSoulsMode
		});
	}

	[[nodiscard]] inline PersistedHostInterfaceSettings
	EncodeNormalizedHostInterfaceSettings(
		const HostInterfaceSettings& a_settings)
	{
		const auto& appearance = a_settings.appearance;
		PersistedHostInterfaceSettings persisted;
		if (appearance.iconColorMode)
		{
			persisted.monochromeIcons =
				*appearance.iconColorMode == Theme::IconColorMode::kMonochrome;
		}
		persisted.sidebarLayout =
			std::string{ SidebarLayoutKindName(a_settings.sidebarLayout) };
		persisted.themePreset = a_settings.themePreset;
		if (appearance.accentColor)
			persisted.accentColor = EncodeHostAccentColor(*appearance.accentColor);
		persisted.windowBackgroundOpacity = appearance.windowBackgroundOpacity;
		if (appearance.paletteBackgroundColor)
		{
			persisted.paletteBackgroundColor =
				EncodeHostAccentColor(*appearance.paletteBackgroundColor);
		}
		persisted.paletteBackgroundOpacity = appearance.paletteBackgroundOpacity;
		persisted.backgroundBlur = appearance.backgroundBlur;
		persisted.backgroundBlurStrength = appearance.backgroundBlurStrength;
		persisted.uiScale = a_settings.uiScale;
		persisted.bodyFontFamily = appearance.bodyFontFamily.value_or(std::string{});
		persisted.monospaceFontFamily =
			appearance.monospaceFontFamily.value_or(std::string{});
		persisted.menuToggleKey = a_settings.menuToggleKey;
		persisted.menuToggleGamepad = a_settings.menuToggleGamepad;
		persisted.feedbackPlacement =
			std::string{ FieldFeedbackLayoutName(a_settings.feedbackPlacement) };
		persisted.fallSoulsMode = a_settings.fallSoulsMode;
		if (appearance.logoColors)
		{
			persisted.logoColors =
				std::string{ LogoColorModeName(*appearance.logoColors) };
		}
		return persisted;
	}

	[[nodiscard]] inline PersistedHostInterfaceSettings EncodeHostInterfaceSettings(
		const HostInterfaceSettings& a_settings)
	{
		return EncodeNormalizedHostInterfaceSettings(
			NormalizeHostInterfaceSettings(a_settings));
	}

	[[nodiscard]] inline HostInterfaceSettings DefaultHostInterfaceSettings()
	{
		return {};
	}

	[[nodiscard]] inline HostInterfacePreviewSettings PreviewHostInterfaceSettings(
		const HostInterfaceSettings& a_settings)
	{
		HostInterfacePreviewSettings preview{
			a_settings.sidebarLayout,
			a_settings.themePreset,
			a_settings.appearance,
			a_settings.feedbackPlacement
		};
		preview.appearance.bodyFontFamily.reset();
		preview.appearance.monospaceFontFamily.reset();
		return preview;
	}

	namespace HostSettings
	{
		void Initialize() noexcept;
		// Advances whenever Current() or EffectivePreview() may change.
		[[nodiscard]] uint64_t Revision() noexcept;
		// Valid on the calling thread until its next call.
		[[nodiscard]] const HostInterfaceSettings& Current() noexcept;
		[[nodiscard]] HostInterfacePreviewSettings EffectivePreview() noexcept;
		[[nodiscard]] bool Apply(HostInterfaceSettings a_settings) noexcept;
		[[nodiscard]] bool SetSidebarLayout(SidebarLayoutKind a_layout) noexcept;
		void SetPreview(
			HostInterfacePreviewSettings a_settings,
			uint64_t a_pageRevision) noexcept;
		void NotifyMenuVisible(bool a_visible) noexcept;
		void SetPageActive(bool a_active) noexcept;
		[[nodiscard]] uint64_t PageRevision() noexcept;
		[[nodiscard]] bool SetHotkeyOverride(
			std::string_view a_id,
			std::string_view a_chord,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
		[[nodiscard]] bool RemoveHotkeyOverride(std::string_view a_id,
			HotkeySlot a_slot = HotkeySlot::kKeyboardMouse) noexcept;
	}
}
