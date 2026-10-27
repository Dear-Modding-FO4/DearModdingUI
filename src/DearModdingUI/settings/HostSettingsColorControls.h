#pragma once

#include <DearModdingUI/settings/HostSettings.h>

#include <optional>

namespace DearModdingUI::HostSettingsViewDetail
{
	[[nodiscard]] bool DrawGameColorSyncControls(
		HostAccentColor& a_color,
		std::optional<HostAccentColor> a_hudColor,
		std::optional<HostAccentColor> a_pipboyColor,
		float a_width) noexcept;
}
