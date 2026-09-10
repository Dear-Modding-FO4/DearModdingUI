#pragma once

#include <DearModdingUI/Client.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace DmuiTestFixtures
{
	struct NavigationFixturePage
	{
		const char* id;
		const char* displayName;
		const char* categoryId;
		const char* categoryDisplayName;
		const char* summary;
	};

	struct NavigationFixtureClient
	{
		const char* id;
		const char* displayName;
		const char* source;
	};

	inline constexpr std::array kNavigationFixturePages{
		NavigationFixturePage{
			"overview",
			"Overview",
			nullptr,
			nullptr,
			"Synthetic navigation-only bridge fixture."
		},
		NavigationFixturePage{
			"tuning",
			"Tuning",
			"configuration",
			"Configuration",
			"Generic bridged configuration controls."
		},
		NavigationFixturePage{
			"diagnostics",
			"Diagnostics",
			"diagnostics",
			"Diagnostics",
			"A category intentionally matching its page name."
		}
	};

	inline constexpr std::array kNavigationFixtureClients{
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.unnamed-a",
			"[Fixture] Navigation Unnamed Alpha",
			""
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.unnamed-b",
			"[Fixture] Navigation Unnamed Beta",
			""
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.long-a",
			"[Fixture] Navigation Long Source Alpha",
			"A Very Long Navigation Bridge Source Label for Layout Stress"
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.long-b",
			"[Fixture] Navigation Long Source Beta",
			"A Very Long Navigation Bridge Source Label for Layout Stress"
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.long-c",
			"[Fixture] Navigation Long Source Gamma",
			"A Very Long Navigation Bridge Source Label for Layout Stress"
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.long-d",
			"[Fixture] Navigation Long Source Delta",
			"A Very Long Navigation Bridge Source Label for Layout Stress"
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.script-a",
			"[Fixture] Navigation Script Alpha",
			"Synthetic Configuration Bridge"
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.script-b",
			"[Fixture] Navigation Script Beta",
			"Synthetic Configuration Bridge"
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.script-c",
			"[Fixture] Navigation Script Gamma",
			"Synthetic Configuration Bridge"
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.script-d",
			"[Fixture] Navigation Script Delta",
			"Synthetic Configuration Bridge"
		},
		NavigationFixtureClient{
			"dearmodding.tests.synthetic.navigation.script-e",
			"[Fixture] Navigation Script Epsilon",
			"Synthetic Configuration Bridge"
		}
	};

	[[nodiscard]] bool RegisterNavigationComparisonFixtures(
		std::vector<std::unique_ptr<dmui::Client>>& a_clients,
		std::string& a_error) noexcept;
}
