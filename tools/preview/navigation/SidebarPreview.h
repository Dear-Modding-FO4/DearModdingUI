#pragma once

#if defined(DMUI_PREVIEW)

#include <DearModdingUI/navigation/SidebarView.h>

namespace DearModdingUI
{
	struct SidebarViewContext;

	void DrawIconRailNavigation(
		const SidebarViewContext& a_context) noexcept;
}

#endif
