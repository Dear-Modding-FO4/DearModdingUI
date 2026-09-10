#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/presentation/Theme.h>

namespace DearModdingUI::Theme
{
	FontGuard::FontGuard(FontRole, float) noexcept {}
	FontGuard::~FontGuard() noexcept = default;

	float Scale() noexcept
	{
		return 1.0f;
	}

	float SearchScale() noexcept
	{
		return kBaselineFontSize / kSearchBaselineFontSize;
	}

	ImVec4 IconTint() noexcept
	{
		return kFullPalette[ImGuiCol_Text];
	}

	DMUI_ThemeColors ColorSnapshot() noexcept
	{
		return MakeColorSnapshot(HostAccentToImVec4(kDefaultHostAccentColor));
	}
}
