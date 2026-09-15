#pragma once

#include <DearModdingUI/settings/HostSettings.h>

#include <optional>

namespace DearModdingUI
{
	enum class GameColorSource
	{
		kHUD,
		kPipboy
	};

	[[nodiscard]] std::optional<HostAccentColor> ReadGameColor(
		GameColorSource a_source) noexcept;
}
