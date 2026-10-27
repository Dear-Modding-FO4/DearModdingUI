#include <DearModdingUI/presentation/ThemeSpec.h>
#include <DearModdingUI/settings/HostSettings.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace DearModdingUI::Theme
{
	namespace
	{
		static_assert(ImGuiCol_COUNT == 63);
		inline constexpr std::array<ImVec4, ImGuiCol_COUNT> kPalette{
			ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
			ImVec4(1.0f, 1.0f, 1.0f, 0.3f),
			ImVec4(0.03f, 0.03f, 0.03f, 0.55f),
			ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
			ImVec4(0.05f, 0.05f, 0.1f, 0.85f),
			ImVec4(0.5f, 0.5f, 0.5f, 0.8f),
			ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
			ImVec4(0.4f, 0.4f, 0.4f, 0.7f),
			ImVec4(0.26f, 0.26f, 0.26f, 0.4f),
			ImVec4(0.4f, 0.4f, 0.4f, 0.45f),
			ImVec4(0.0f, 0.0f, 0.0f, 0.83f),
			ImVec4(0.0f, 0.0f, 0.0f, 0.87f),
			ImVec4(0.2f, 0.2f, 0.3f, 0.9f),
			ImVec4(0.02f, 0.02f, 0.03f, 0.9f),
			ImVec4(0.2f, 0.22f, 0.27f, 0.9f),
			ImVec4(0.28f, 0.28f, 0.28f, 1.0f),
			ImVec4(0.42f, 0.42f, 0.42f, 1.0f),
			ImVec4(0.56f, 0.56f, 0.56f, 1.0f),
			ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
			ImVec4(0.31f, 0.31f, 0.31f, 0.5f),
			ImVec4(0.26f, 0.98f, 0.3752f, 1.0f),
			ImVec4(0.45f, 1.0f, 0.55f, 1.0f),
			ImVec4(0.26f, 0.98f, 0.3752f, 0.39f),
			ImVec4(0.26f, 0.98f, 0.3752f, 0.2f),
			ImVec4(0.26f, 0.98f, 0.3752f, 0.59f),
			ImVec4(0.06f, 0.98f, 0.2072f, 0.39f),
			ImVec4(0.26f, 0.98f, 0.3752f, 0.2f),
			ImVec4(0.26f, 0.98f, 0.3752f, 0.59f),
			ImVec4(0.5f, 0.5f, 0.5f, 0.6f),
			ImVec4(0.7f, 0.6f, 0.6f, 1.0f),
			ImVec4(0.9f, 0.7f, 0.7f, 1.0f),
			ImVec4(0.6f, 0.6f, 0.6f, 0.8f),
			ImVec4(0.6f, 0.6f, 0.6f, 0.1f),
			ImVec4(0.6f, 0.6f, 0.6f, 0.1f),
			ImVec4(0.9f, 0.9f, 0.9f, 1.0f),
			ImVec4(0.26f, 0.98f, 0.3752f, 0.31f),
			ImVec4(0.26f, 0.98f, 0.3752f, 0.8f),
			ImVec4(0.26f, 0.98f, 0.3752f, 1.0f),
			ImVec4(0.38f, 0.83f, 0.452f, 1.0f),
			ImVec4(0.15f, 0.15f, 0.15f, 0.97f),
			ImVec4(0.26f, 0.98f, 0.3752f, 1.0f),
			ImVec4(0.5f, 0.5f, 0.5f, 0.0f),
			ImVec4(0.7f, 0.6f, 0.6f, 0.5f),
			ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
			ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
			ImVec4(0.9f, 0.7f, 0.0f, 1.0f),
			ImVec4(0.9f, 0.7f, 0.0f, 1.0f),
			ImVec4(0.9f, 0.7f, 0.0f, 1.0f),
			ImVec4(0.26f, 0.98f, 0.3752f, 0.4f),
			ImVec4(0.26f, 0.26f, 0.26f, 1.0f),
			ImVec4(0.19f, 0.19f, 0.19f, 1.0f),
			ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
			ImVec4(1.0f, 1.0f, 1.0f, 0.06f),
			ImVec4(0.38f, 0.83f, 0.452f, 1.0f),
			ImVec4(0.26f, 0.98f, 0.3752f, 0.35f),
			ImVec4(0.7f, 0.7f, 0.7f, 0.65f),
			ImVec4(0.8f, 0.5f, 0.5f, 1.0f),
			ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
			ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
			ImVec4(0.26f, 0.98f, 0.3752f, 1.0f),
			ImVec4(0.3f, 0.3f, 0.3f, 0.56f),
			ImVec4(0.2f, 0.2f, 0.2f, 0.35f),
			ImVec4(0.2f, 0.2f, 0.2f, 0.35f)
		};

		// These slots follow the accent hue and keep their own alpha.
		inline constexpr ImGuiCol kAccentColors[]{
			ImGuiCol_CheckboxSelectedBg,
			ImGuiCol_SliderGrab,
			ImGuiCol_SliderGrabActive,
			ImGuiCol_Button,
			ImGuiCol_ButtonHovered,
			ImGuiCol_ButtonActive,
			ImGuiCol_Header,
			ImGuiCol_HeaderHovered,
			ImGuiCol_HeaderActive,
			ImGuiCol_InputTextCursor,
			ImGuiCol_TabHovered,
			ImGuiCol_Tab,
			ImGuiCol_TabSelected,
			ImGuiCol_TabSelectedOverline,
			ImGuiCol_TabDimmedSelected,
			ImGuiCol_TabDimmedSelectedOverline,
			ImGuiCol_DockingPreview,
			ImGuiCol_TextLink,
			ImGuiCol_TextSelectedBg,
			ImGuiCol_DragDropTarget,
			ImGuiCol_DragDropTargetBg,
			ImGuiCol_UnsavedMarker,
			ImGuiCol_NavCursor
		};

		inline constexpr float kMutedAccentOpacity{ 0.39f };
		inline constexpr std::string_view kBundledFamily{ "Jost" };

		[[nodiscard]] RoleTypography RoleOf(
			std::string_view a_family,
			FontCatalog::FontWeight a_weight,
			float a_sizeScale)
		{
			return { std::string{ a_family }, a_weight, false, a_sizeScale };
		}

		[[nodiscard]] Typography MakeTypography()
		{
			using FontCatalog::FontWeight;
			return {
				kDefaultFontRatio,
				{
					RoleOf(kDefaultBodyFontFamily, FontWeight::kRegular, 1.0f),
					RoleOf(kBundledFamily, FontWeight::kSemiBold, 1.3f),
					RoleOf(kBundledFamily, FontWeight::kRegular, 1.0f),
					RoleOf(kBundledFamily, FontWeight::kRegular, 1.0f),
					RoleOf(kBundledFamily, FontWeight::kRegular, 0.9f),
					RoleOf(kDefaultMonospaceFontFamily, FontWeight::kRegular, 0.9f)
				}
			};
		}

		[[nodiscard]] constexpr ImVec4 FromBytes(
			uint8_t a_red,
			uint8_t a_green,
			uint8_t a_blue) noexcept
		{
			constexpr auto inverseByte = 1.0f / 255.0f;
			return {
				static_cast<float>(a_red) * inverseByte,
				static_cast<float>(a_green) * inverseByte,
				static_cast<float>(a_blue) * inverseByte,
				1.0f
			};
		}

		[[nodiscard]] ImGuiStyle MakeStyle(const ImVec4& a_accent) noexcept
		{
			ImGuiStyle style{};
			style.WindowBorderSize = 2.0f;
			style.ChildBorderSize = 0.0f;
			style.FrameBorderSize = 1.0f;
			style.WindowPadding = { 8.0f, 8.0f };
			style.WindowRounding = 12.0f;
			style.IndentSpacing = 8.0f;
			style.FramePadding = { 8.0f, 4.0f };
			style.CellPadding = { 8.0f, 2.0f };
			style.ItemSpacing = { 4.0f, 8.0f };
			style.FrameRounding = 4.0f;
			style.TabRounding = 4.0f;
			style.ScrollbarRounding = 9.0f;
			style.ScrollbarSize = 12.0f;
			style.GrabRounding = 3.0f;
			style.GrabMinSize = 12.0f;
			style.HoverDelayNormal = 0.1f;

			auto palette = kPalette;
			palette[ImGuiCol_WindowBg].w = kDefaultWindowBackgroundOpacity;
			palette[ImGuiCol_ScrollbarBg].w = 0.0f;
			palette[ImGuiCol_ScrollbarGrab].w = 0.5f;
			palette[ImGuiCol_ScrollbarGrabHovered].w = 0.75f;
			palette[ImGuiCol_ScrollbarGrabActive].w = 0.9f;
			palette[ImGuiCol_PopupBg] =
				HostAccentToImVec4(kDefaultPaletteBackgroundColor);
			palette[ImGuiCol_PopupBg].w = kDefaultPaletteBackgroundOpacity;
			std::ranges::copy(palette, style.Colors);
			RetintAccentColors(style, a_accent);
			return style;
		}

		[[nodiscard]] Tokens MakeTokens(const ImVec4& a_accent) noexcept
		{
			Tokens tokens{};
			tokens.text = kPalette[ImGuiCol_Text];
			tokens.muted = { 0.5f, 0.5f, 0.5f, 1.0f };
			tokens.accent = a_accent;
			tokens.info = { 0.2f, 1.0f, 0.328f, 1.0f };
			tokens.success = { 0.0f, 1.0f, 0.0f, 1.0f };
			tokens.warning = { 1.0f, 0.6f, 0.2f, 1.0f };
			tokens.error = { 1.0f, 0.4f, 0.4f, 1.0f };
			tokens.disabled = { 0.5f, 0.5f, 0.5f, 1.0f };
			tokens.restartNeeded = { 0.4f, 1.0f, 0.4f, 1.0f };
			tokens.currentHotkey = { 1.0f, 1.0f, 0.0f, 1.0f };
			tokens.heading = { 0.8f, 0.8f, 0.8f, 1.0f };
			tokens.headingHovered = { 0.6f, 0.6f, 0.6f, 1.0f };
			tokens.searchFrameActive = { 0.3f, 0.3f, 0.3f, 0.9f };
			tokens.rowAlt = kPalette[ImGuiCol_TableRowBgAlt];
			tokens.logoTop = FromBytes(0x9E, 0x00, 0x5D);
			tokens.logoLeft = FromBytes(0x19, 0x7B, 0x30);
			tokens.logoRight = FromBytes(0x00, 0x4A, 0x80);
			tokens.logoBottom = FromBytes(0xF7, 0x94, 0x1D);
			return tokens;
		}
	}

	void RetintAccentColors(
		ImGuiStyle& a_style,
		const ImVec4& a_accent) noexcept
	{
		for (const auto color : kAccentColors)
		{
			a_style.Colors[color].x = std::clamp(a_accent.x, 0.0f, 1.0f);
			a_style.Colors[color].y = std::clamp(a_accent.y, 0.0f, 1.0f);
			a_style.Colors[color].z = std::clamp(a_accent.z, 0.0f, 1.0f);
		}
	}

	void DeriveTokens(
		ThemeSpec& a_theme,
		bool a_keepAccentMuted,
		bool a_keepIconTint) noexcept
	{
		auto& tokens = a_theme.tokens;
		if (!a_keepAccentMuted)
		{
			tokens.accentMuted = tokens.accent;
			tokens.accentMuted.w = a_theme.chrome.mutedAccentOpacity;
		}
		if (!a_keepIconTint)
		{
			tokens.iconTint =
				a_theme.effects.iconColorMode == IconColorMode::kColored ?
				tokens.accent :
				tokens.text;
		}
	}

	ThemeSpec MakeDefaultTheme() noexcept
	{
		return MakeDefaultTheme(HostAccentToImVec4(kDefaultHostAccentColor));
	}

	ThemeSpec MakeDefaultTheme(const ImVec4& a_accent) noexcept
	{
		ThemeSpec theme{
			MakeStyle(a_accent),
			MakeTokens(a_accent),
			{
				.headerFallbackTextScale = 1.5f,
				.separatorThickness = 3.0f,
				.featureTitleScale = 1.5f,
				.versionTextOpacity = 0.6f,
				.headingMinimizedFactor = 0.7f,
				.searchIconSize = 20.0f,
				.searchIconAlpha = 0.7f,
				.searchIconOffsetX = 8.0f,
				.searchInputPaddingExtra = 14.0f,
				.searchInputFramePaddingY = 6.0f,
				.searchIconStrokeRatio = 0.11f,
				.mutedAccentOpacity = kMutedAccentOpacity,
				.sectionGapScale = 1.0f,
				.panelPadding = { 12.0f, 12.0f }
			},
			{
				.backgroundBlur = true,
				.backgroundBlurStrength = kDefaultBackgroundBlurStrength,
				.iconColorMode = IconColorMode::kColored,
				.logoColorMode = LogoColorMode::kOriginal
			},
			MakeTypography(),
			{ .baselineOffsetRatio = 4.0f / 21.0f }
		};
		DeriveTokens(theme, false, false);
		return theme;
	}
}
