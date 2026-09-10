#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/controls/ChromeGeometry.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	void run_host_control_checks(Runner& runner)
	{
		runner.test("host close and footer gear stay clear of adjacent content", [] {
			require(ShouldDrawHeaderClose(false, true),
				"undocked titleless host lost its close button");
			require(
					!ShouldDrawHeaderClose(true, true) &&
						!ShouldDrawHeaderClose(true, false) &&
						!ShouldDrawHeaderClose(false, false),
					"host close duplicated a native or docked close affordance");
			struct Case
			{
				float fontSize;
				float uiScale;
			};
			constexpr std::array cases{
				Case{ 16.0f, 1.0f },
				Case{ 18.0f, 1.25f },
				Case{ 21.0f, 1.5f },
				Case{ 28.0f, 2.0f }
			};
			for (const auto& test : cases)
			{
				const auto fontSize = test.fontSize * test.uiScale;
				const auto padding = 2.0f * test.uiScale;
				const auto spacing = 8.0f * test.uiScale;
				const auto iconSize = HostChromeIconSize(fontSize);
				const auto extent = HostChromeButtonExtent(fontSize, padding);
				const auto header = ResolveTrailingControlLayout(
					24.0f, 1896.0f, extent, spacing);
				const auto footer = ResolveTrailingControlLayout(
					36.0f, 1264.0f, extent, spacing);
				require(
						iconSize == fontSize * 1.5f &&
							extent > TitleBarButtonExtent(fontSize, padding),
						"host chrome did not use its larger icon scale");
				require(
						header.controlMaxX == 1896.0f &&
							header.controlMinX == 1896.0f - extent &&
							header.adjacentMaxX ==
								header.controlMinX - spacing,
						"close button geometry changed");
				require(
						footer.controlMaxX == 1264.0f &&
							footer.controlMinX == 1264.0f - extent &&
							footer.adjacentMaxX ==
								footer.controlMinX - spacing,
						"footer gear geometry changed");
				require(
						header.adjacentMaxX <= header.controlMinX &&
							footer.adjacentMaxX <= footer.controlMinX,
						"host chrome overlapped adjacent content");
			}
		});

	}
}
