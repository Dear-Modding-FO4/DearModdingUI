#pragma once

#include <DearModdingUI/navigation/Navigation.h>

#include <map>
#include <string>

namespace DearModdingUI
{
	struct HostPageViewState
	{
		std::map<std::string, bool> diagnosticExpansion;
	};

	void DrawHostPage(
		HostPageKind a_page,
		HostPageViewState& a_state) noexcept;
}
