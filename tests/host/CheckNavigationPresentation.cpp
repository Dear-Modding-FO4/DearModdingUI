#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/navigation/NavigationPresentation.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/VisualDecisions.h>
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

	void run_navigation_presentation_checks(Runner& runner)
	{
		runner.test("one-page navigation and failed-page presentation remain stable", [] {
			Registry registry;
			CallbackState state;
			const auto client = AddClient(registry, "single.mod", "Single", state);
			AddCategory(registry, client, "general", "General");
			const auto page = AddPage(registry, client, "only", "Only", "general", 0,
				DMUI_PAGE_KIND_SETTINGS, state);
			require(registry.Freeze(), "registry did not freeze");
			const auto& navigation = registry.Navigation();
			require(navigation.clients.size() == 1, "single client was omitted");
			require(navigation.clients[0].categories.size() == 1, "single category was omitted");
			require(navigation.FirstPage() == page, "single page was not the fallback");
			require(DecidePagePresentation(navigation.FindPage(page), false) ==
					PagePresentation::kContent,
				"healthy page did not present content");
			require(DecidePagePresentation(navigation.FindPage(page), true) ==
					PagePresentation::kFailure,
				"failed page did not present a stable error");
			registry.MarkPageFailed(page);
			require(registry.PageFailed(page), "failed page state was not retained");
			require(registry.HasSettingsPages(), "failed page removed the host's settings shell");
			require(DecidePagePresentation(nullptr, false) == PagePresentation::kEmpty,
				"missing page did not present an empty state");
		});

		runner.test("theme scaling clamps and composes user scale", [] {
			const auto minimum = Theme::ResolveFontSize(1);
			const auto baseline = Theme::ResolveFontSize(1080);
			const auto highResolution = Theme::ResolveFontSize(2160);
			const auto maximum = Theme::ResolveFontSize(8640);
			require(
				minimum == Theme::kMinFontSize &&
					baseline > minimum &&
					highResolution > baseline &&
					maximum == Theme::kMaxFontSize,
				"font scaling lost its resolution boundaries");
			require(
				ResolveUiScale(1.0f, 1080) == 1.0f &&
					ResolveUiScale(2.0f, 1080) == 1.0f &&
					ResolveUiScale(1.0f, 2160) == 2.0f &&
					ResolveUiScale(
						1.0f,
						1080,
						Theme::kMaxUserScale) == 2.0f,
				"resolution and accessibility scaling no longer compose");
		});

		runner.test("absent icons reserve no navigation layout space", [] {
			const auto absent = DecideInlineIconLayout(false, 80.0f, 20.0f, 20.0f, 4.0f);
			require(!absent.drawIcon &&
					absent.iconSize == 0.0f &&
					absent.textOffset == 0.0f &&
					absent.contentWidth == 80.0f &&
					absent.contentHeight == 20.0f,
				"absent icon left a blank layout box");

			const auto present = DecideInlineIconLayout(true, 80.0f, 18.0f, 20.0f, 4.0f);
			require(present.drawIcon &&
					present.iconSize == 20.0f &&
					present.textOffset == 24.0f &&
					present.contentWidth == 104.0f &&
					present.contentHeight == 20.0f,
				"present icon layout did not align to the font");
		});

	}
}
