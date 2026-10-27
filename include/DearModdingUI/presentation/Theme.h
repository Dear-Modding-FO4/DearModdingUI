#pragma once

#include <DearModdingUI/presentation/FontMetrics.h>
#include <DearModdingUI/StyledText.h>
#include <DearModdingUI/presentation/ThemeLayout.h>
#include <DearModdingUI/presentation/ThemeSpec.h>

#include <imgui/imgui.h>

#include <array>
#include <memory>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI::Theme
{
	struct ThemePreset;
	struct ThemePresetInfo;

	enum class FontStyle : uint8_t
	{
		kRegular,
		kBold,
		kItalic,
		kBoldItalic
	};

	class FontGuard
	{
	public:
		explicit FontGuard(
			FontRole a_role,
			FontStyle a_style = FontStyle::kRegular,
			float a_scale = 1.0f) noexcept;
		~FontGuard() noexcept;

		FontGuard(const FontGuard&) = delete;
		FontGuard& operator=(const FontGuard&) = delete;

	private:
		bool m_pushed{ false };
	};

	void Initialize(void* a_window) noexcept;
	[[nodiscard]] bool PrepareFrame(uint32_t a_backBufferHeight) noexcept;
	void ApplyStyle() noexcept;
	[[nodiscard]] bool FontsReady() noexcept;
	[[nodiscard]] float FontSize(FontRole a_role) noexcept;
	// Bold raises the role weight to at least Bold; italic forces the slant.
	[[nodiscard]] bool PushFont(
		FontRole a_role,
		FontStyle a_style = FontStyle::kRegular,
		float a_scale = 1.0f) noexcept;
	void PopFont() noexcept;
	[[nodiscard]] float Scale() noexcept;
	[[nodiscard]] float SearchScale() noexcept;

	struct IconGlyphRef
	{
		// Null when no font in play can draw the icon.
		ImFont* font;
		ImWchar glyph;
	};
	// Prefers the set's mapped glyph, then the Phosphor glyph in a_current.
	[[nodiscard]] inline IconGlyphRef ResolveIconIn(
		const IconSet& a_icons,
		ImFont* a_iconFont,
		ImFont* a_current,
		char32_t a_phosphor) noexcept
	{
		if (!dmui::IsRepresentableIconGlyph<ImWchar>(a_phosphor))
			return {};
		if (a_iconFont)
		{
			const auto mapped = a_icons.Find(a_phosphor);
			if (mapped != 0 && dmui::IsRepresentableIconGlyph<ImWchar>(mapped) &&
				a_iconFont->IsGlyphInFont(static_cast<ImWchar>(mapped)))
				return { a_iconFont, static_cast<ImWchar>(mapped) };
		}
		const auto glyph = static_cast<ImWchar>(a_phosphor);
		return a_current && a_current->IsGlyphInFont(glyph) ?
			IconGlyphRef{ a_current, glyph } :
			IconGlyphRef{};
	}
	// Maps a Phosphor codepoint to the theme icon set or the current font.
	[[nodiscard]] IconGlyphRef ResolveIcon(char32_t a_phosphor) noexcept;
	[[nodiscard]] DMUI_ThemeTokens TokenSnapshot() noexcept;
	[[nodiscard]] ImVec4 TextColor(dmui::TextTone a_tone) noexcept;
	[[nodiscard]] dmui::TextTone StatusTone(DMUI_Severity a_severity) noexcept;
	[[nodiscard]] ImVec4 StatusTextColor(DMUI_Severity a_severity) noexcept;
	// Re-reads the presets folder; the settings page calls it on entry.
	void RefreshThemePresets() noexcept;
	[[nodiscard]] const std::vector<ThemePresetInfo>& AvailableThemePresets() noexcept;
	// The preset's resolved theme before user overrides; never null.
	[[nodiscard]] std::shared_ptr<const ThemePreset> ResolveThemePreset(
		std::string_view a_id) noexcept;
	// The family folders under Fonts, led by the built-in face on request.
	[[nodiscard]] std::vector<std::string> AvailableFontFamilies(bool a_includeBuiltin);
	// The requested family when installed, else a_fallback.
	[[nodiscard]] std::string_view ResolveFontFamily(
		std::string_view a_requested,
		std::string_view a_fallback) noexcept;
	// Body or monospace: the family the atlas was last built with.
	[[nodiscard]] std::string_view EffectiveFontFamily(FontRole a_role) noexcept;
}
