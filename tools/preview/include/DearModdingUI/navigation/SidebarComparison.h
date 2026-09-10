#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/navigation/NavigationPresentation.h>
#include <DearModdingUI/navigation/Sidebar.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace DearModdingUI
{
#if defined(DMUI_PREVIEW)
	[[nodiscard]] constexpr std::optional<NavigationPresentationKind>
		ParsePreviewNavigationKind(std::string_view a_name) noexcept
	{
		if (a_name == "grouped")
			return NavigationPresentationKind::Grouped;
#if defined(DMUI_PREVIEW)
		if (a_name == "destinations")
			return NavigationPresentationKind::Destinations;
#endif
		return std::nullopt;
	}

	[[nodiscard]] constexpr std::optional<DMUI_ClientOrigin>
		ParsePreviewNavigationOrigin(std::string_view a_name) noexcept
	{
		if (a_name == "native")
			return DMUI_CLIENT_ORIGIN_NATIVE;
		if (a_name == "bridged")
			return DMUI_CLIENT_ORIGIN_BRIDGED;
		return std::nullopt;
	}

	void ConfigurePreviewSidebarComparison(
		std::optional<SidebarLayoutKind> a_layoutOverride,
		bool a_overrideExpandedClients,
		std::span<const std::string> a_expandedClients,
		std::optional<NavigationPresentationKind> a_presentationOverride =
			std::nullopt,
		DMUI_ClientOrigin a_destinationOrigin = DMUI_CLIENT_ORIGIN_NATIVE);
#endif
}
