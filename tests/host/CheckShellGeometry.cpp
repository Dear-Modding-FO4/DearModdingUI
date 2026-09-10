#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/controls/ChromeGeometry.h>
#include <DearModdingUI/navigation/Sidebar.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace std::string_view_literals;
	using namespace DearModdingUI;
	using namespace support::host;

	void run_shell_geometry_checks(Runner& runner)
	{
		runner.test("footer controls clamp, separate, and scale", [] {
			const auto narrow = ResolveFooterControlsLayout(
				20.0f,
				100.0f,
				60.0f,
				50.0f,
				8.0f);
			require(
					narrow.runMaxX == 20.0f &&
						narrow.dismissMinX >= 20.0f &&
						narrow.dismissMaxX + 8.0f <=
							narrow.settingsMinX &&
						narrow.settingsMaxX <= 100.0f,
					"narrow footer controls escaped their bounds");

			const auto base = ResolveFooterControlsLayout(
				0.0f,
				600.0f,
				40.0f,
				28.0f,
				8.0f);
			const auto scaled = ResolveFooterControlsLayout(
				0.0f,
				1200.0f,
				80.0f,
				56.0f,
				16.0f);
			require(
					scaled ==
						FooterControlsLayout{
							base.runMaxX * 2.0f,
							base.dismissMinX * 2.0f,
							base.dismissMaxX * 2.0f,
							base.settingsMinX * 2.0f,
							base.settingsMaxX * 2.0f
						},
					"footer controls did not follow the style scale");

			require(
				base.runMaxX <= base.dismissMinX &&
					base.dismissMaxX <= base.settingsMinX,
				"footer controls overlapped at baseline scale");
		});

		runner.test("two-pane sidebar preserves a measured page region", [] {
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						2,
						240.0f) == SidebarPaneHeights{ 120.0f, 880.0f },
					"two mods consumed a proportional half of the sidebar");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						10,
						240.0f) == SidebarPaneHeights{ 600.0f, 400.0f },
					"ten mods did not retain their measured list height");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						20,
						240.0f) == SidebarPaneHeights{ 760.0f, 240.0f },
					"overflowing mods displaced the minimum page region");
			require(
					ResolveSidebarPaneHeights(
						-100.0f,
						60.0f,
						10,
						240.0f) == SidebarPaneHeights{},
					"negative space produced pane height");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						10,
						40.0f,
						3,
						240.0f) == SidebarPaneHeights{ 720.0f, 280.0f },
					"navigation headings were omitted from the mod pane budget");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						20,
						40.0f,
						3,
						240.0f) == SidebarPaneHeights{ 760.0f, 240.0f },
					"navigation headings displaced the minimum page region");
			require(
					ResolveSidebarPaneHeights(
						1000.0f,
						60.0f,
						8,
						40.0f,
						2,
						48.0f,
						240.0f) == SidebarPaneHeights{ 608.0f, 392.0f },
					"source controls were omitted from the mod pane budget");
		});

		runner.test("sidebar layout names parse and round trip", [] {
			for (const auto& layout : SIDEBAR_LAYOUTS)
			{
				require(
					ParseSidebarLayout(layout.id) == layout.kind &&
						SidebarLayoutKindName(layout.kind) == layout.id &&
						layout.preview,
					"sidebar layout name did not round trip");
			}
			require(
				!ParseSidebarLayout("columns") &&
					SidebarLayoutKindName(
						static_cast<SidebarLayoutKind>(99)) == "unknown" &&
					DEFAULT_SIDEBAR_LAYOUT == SidebarLayoutKind::Tree,
				"unknown or default sidebar layout handling changed");

			PersistedHostInterfaceSettings persisted;
			for (const auto value : { ""sv, "columns"sv, "iconrail"sv })
			{
				persisted.sidebarLayout = value;
				require(
					DecodeHostInterfaceSettings(persisted).sidebarLayout ==
						SidebarLayoutKind::Tree,
					"unavailable sidebar config did not fall back to tree");
			}
			require(
				!ParseUserSidebarLayout("iconrail"),
				"icon rail was accepted as a user setting");
		});

	}
}
