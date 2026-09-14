#include "../Harness.h"
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/VisualDecisions.h>

namespace vmm_tests
{
	using namespace DearModdingUI;

	void run_navigation_presentation_checks(Runner& runner)
	{
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
