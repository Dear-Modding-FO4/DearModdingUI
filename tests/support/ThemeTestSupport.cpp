#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/presentation/Theme.h>

namespace DearModdingUI::Theme
{
	FontGuard::FontGuard(FontRole, float) noexcept {}
	FontGuard::~FontGuard() noexcept = default;

	DMUI_ThemeColors ColorSnapshot() noexcept
	{
		return MakeColorSnapshot(HostAccentToImVec4(kDefaultHostAccentColor));
	}
}
