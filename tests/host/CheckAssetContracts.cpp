#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/presentation/FontCatalog.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/navigation/Navigation.h>
#include <DearModdingUI/SettingsActions.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/ThemeDefaults.h>
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

	void run_asset_contract_checks(Runner& runner)
	{
		runner.test("Phosphor manifest matches shipped font", [] {
			require(
				!kPhosphorIconGlyphs.empty() &&
					FindPhosphorIconGlyphOrZero("gear") ==
						PhosphorGlyph::kGear,
				"generated Phosphor resolver lost a representative icon");
			const auto font = std::filesystem::current_path() /
				"data/F4SE/Plugins/DearModdingUI/Fonts/Phosphor/Phosphor-Fill.ttf";
			require(
				Sha256(font) ==
					"a53f5d2630cab5e3b7536ecb9d69d71519a2190298c22b1f8d770dd37bc2940a",
				"Phosphor Fill font no longer matches @phosphor-icons/web@2.1.2");
		});

		runner.test("icon resolution follows semantic fallback chain", [] {
			const auto cloudSun =
				FindPhosphorIconGlyphOrZero("cloud-sun");
			const auto sunHorizon =
				FindPhosphorIconGlyphOrZero("sun-horizon");
			const auto lightbulb =
				FindPhosphorIconGlyphOrZero("lightbulb");
			require(
				ResolveIconGlyph(IconKind::kClient, "acorn") ==
						FindPhosphorIconGlyphOrZero("acorn") &&
					ResolveIconGlyph(IconKind::kClient, "acorn") != char32_t{},
				"full generated icon catalog was not consulted");
			require(ResolveIconGlyph(IconKind::kCategory, "gear") ==
					PhosphorGlyph::kGear,
				"category did not prefer an explicit icon name");
			require(ResolveClientIconGlyph(
						"puzzle-piece",
						"Performance",
						"Weather Overhaul") ==
					PhosphorGlyph::kPuzzlePiece,
				"explicit client icon did not win");
			const auto performance =
				FindPhosphorIconGlyphOrZero("speedometer");
			const auto weather = FindPhosphorIconGlyphOrZero("cloud-sun");
			require(ResolveClientIconGlyph({}, "Performance", "Unknown") ==
					performance,
				"client category concept was not inferred");
			require(ResolveClientIconGlyph({}, {}, "Weather Overhaul") == weather,
				"whole-word display-name concept was not inferred");
			require(ResolveClientIconGlyph({}, {}, "Weathering Steel") ==
					PhosphorGlyph::kQuestion,
				"display-name inference matched a concept substring");
			require(ResolveClientIconGlyph(
						{},
						{},
						"Audio Performance Toolkit") == performance,
				"longest deterministic concept did not win");
			require(ResolveClientIconGlyph(
						{},
						{},
						"Lighting Graphics Toolkit") ==
					FindPhosphorIconGlyphOrZero("image"),
				"equal-length concepts did not use the stable lexical tie-break");
			require(ResolveActionIconGlyph("clipboard-text") ==
						PhosphorGlyph::kClipboardText &&
					ResolveActionIconGlyph("trash") ==
						PhosphorGlyph::kTrash &&
					ResolveActionIconGlyph("arrow-counter-clockwise") ==
						PhosphorGlyph::kArrowCounterClockwise,
				"canonical action icon names did not resolve");
			require(ResolveActionIconGlyph("unknown") == char32_t{} &&
					ResolveActionIconGlyph("clear-cache") == char32_t{} &&
					ResolveActionIconGlyph("restore-settings") == char32_t{} &&
					ResolveClientIconGlyph({}, {}, "Unknown") ==
						PhosphorGlyph::kQuestion,
				"action and client misses lost distinct fallbacks");
			require(
				ResolveCategoryIconGlyph(
					"Lighting",
					"Community Shaders",
					"dear-modding.community-shaders",
					"cloud-sun",
					"Sun Horizon") == sunHorizon &&
					ResolveCategoryIconGlyph(
						"Lighting",
						"Community Shaders",
						"dear-modding.community-shaders",
						"cloud-sun",
						"unknown") == lightbulb &&
					ResolveClientIconGlyph(
						"cloud-sun",
						"Lighting",
						"Community Shaders") == cloudSun,
				"category override and client icon selection became coupled");
			require(
				ResolveIconGlyph(
					IconKind::kCategory,
					"unknown",
					"Lighting") == lightbulb,
				"category semantic fallback ignored its metadata");
			NavigationClient navigationClient;
			navigationClient.id = "dear-modding.community-shaders";
			navigationClient.displayName = "Community Shaders";
			navigationClient.iconName = "cloud-sun";
			NavigationCategory navigationCategory;
			navigationCategory.id = "lighting";
			navigationCategory.displayName = "Lighting";
			navigationCategory.iconName = "sun-horizon";
			navigationClient.categories.push_back(navigationCategory);
			require(
				ResolveNavigationClientIconGlyph(navigationClient) == cloudSun &&
					ResolveNavigationCategoryIconGlyph(
						navigationClient,
						navigationClient.categories.front()) == sunHorizon,
				"navigation client and category resolvers lost independent overrides");
			NavigationSearchEntry page;
			page.kind = NavigationItemKind::kPage;
			page.displayName = "Unknown";
			page.iconName = "sun-horizon";
			page.category = "Lighting";
			require(
				ResolveNavigationSearchEntryGlyph(page) == sunHorizon,
				"explicit page palette icon did not win");
			page.iconName = "unknown";
			page.displayName = "Lighting Feature";
			require(
				ResolveNavigationSearchEntryGlyph(page) == lightbulb,
				"page-name palette inference did not follow explicit fallback");
			page.displayName = "Unknown";
			page.category = "Weather";
			require(
				ResolveNavigationSearchEntryGlyph(page) == weather,
				"page category metadata was not inferred");
			page.category = "Unknown";
			require(
				ResolveNavigationSearchEntryGlyph(page) ==
					PhosphorGlyph::kFiles,
				"page palette miss lost the Files fallback");

			NavigationSearchEntry action;
			action.kind = NavigationItemKind::kAction;
			action.displayName = "Copy Records";
			action.iconName = "trash";
			require(
				ResolveNavigationSearchEntryGlyph(action) ==
					PhosphorGlyph::kTrash,
				"explicit action palette icon did not win");
			action.iconName = "unknown";
			action.displayName = "Audio Feature";
			require(
				ResolveNavigationSearchEntryGlyph(action) ==
					PhosphorGlyph::kSpeakerHigh,
				"action-label palette inference did not run");
			action.displayName = "Run";
			require(
				ResolveNavigationSearchEntryGlyph(action) ==
					PhosphorGlyph::kTerminalWindow,
				"action palette miss lost the terminal fallback");
			require(
				ResolveActionIconGlyph("unknown") == char32_t{},
				"toolbar actions stopped preserving their text-only contract");
			for (const auto settingsAction : kSettingsActionOrder)
			{
				const auto icon =
					ResolveSettingsActionButtonPresentation(
						settingsAction,
						true);
				const auto fallback =
					ResolveSettingsActionButtonPresentation(
						settingsAction,
						false);
				require(
					icon.glyph == SettingsActionGlyph(settingsAction) &&
						!icon.useTextFallback &&
						fallback.glyph == char32_t{} &&
						fallback.useTextFallback,
					"missing settings glyph did not select text fallback");
			}
		});

		runner.test("raw icon glyph validation rejects truncating code points", [] {
			require(
				IsRepresentableIconGlyph<ImWchar>(PhosphorGlyph::kSun) &&
					!IsRepresentableIconGlyph<ImWchar>(char32_t{}) &&
					!IsRepresentableIconGlyph<ImWchar>(
						char32_t{ 0x1E472 }) &&
					!IsValidUnicodeScalar(char32_t{ 0xD800 }),
				"raw glyph validation allowed zero, invalid, or truncating values");
		});

		runner.test("host colors preserve bytes and select icon tint", [] {
			constexpr std::array colors{
				HostAccentColor{ 0x00, 0x00, 0x00 },
				HostAccentColor{ 0x42, 0xFA, 0x60 },
				HostAccentColor{ 0x56, 0xB4, 0xE9 },
				HostAccentColor{ 0xFF, 0xFF, 0xFF }
			};
			for (const auto color : colors)
			{
				require(
					HostAccentFromImVec4(HostAccentToImVec4(color)) == color,
					"color editor conversion changed a stored component");
			}
			const auto accent = HostAccentToImVec4(colors[1]);
			const ImVec4 text{ 1.0f, 1.0f, 1.0f, 1.0f };
			require(
				SameColor(
					Theme::ResolveIconTint(
						Theme::IconColorMode::kColored,
						accent,
						text),
					accent) &&
					SameColor(
						Theme::ResolveIconTint(
							Theme::IconColorMode::kMonochrome,
							accent,
							text),
						text),
				"icon color mode did not select accent or text tint");
		});

	}
}
