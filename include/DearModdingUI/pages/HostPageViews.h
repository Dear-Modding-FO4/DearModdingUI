#pragma once

#include <DearModdingUI/navigation/Navigation.h>

#include <deque>
#include <map>
#include <string>

namespace DearModdingUI
{
	struct HostPageViewState
	{
		std::map<std::string, bool> diagnosticExpansion;
		// One flag per FAQ entry; a deque keeps the flags in place as it grows.
		std::deque<bool> faqExpansion;
	};

	void DrawHostPage(
		HostPageKind a_page,
		HostPageViewState& a_state) noexcept;
}
