#include <DearModdingUI/presentation/Theme.h>

namespace DearModdingUI::Theme
{
	FontGuard::FontGuard(FontRole, FontStyle, float) noexcept {}
	FontGuard::~FontGuard() noexcept = default;

	float Scale() noexcept
	{
		return 1.0f;
	}

	float SearchScale() noexcept
	{
		return kBaselineFontSize / kSearchBaselineFontSize;
	}

	IconGlyphRef ResolveIcon(char32_t a_phosphor) noexcept
	{
		return ResolveIconIn(
			Current().icons,
			nullptr,
			ImGui::GetFont(),
			a_phosphor);
	}

	const ThemeSpec& Current() noexcept
	{
		static const auto theme = MakeDefaultTheme();
		return theme;
	}
}
