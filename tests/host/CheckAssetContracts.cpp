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
			const auto wrench = FindPhosphorIconGlyphOrZero("wrench");
			const auto hammer = FindPhosphorIconGlyphOrZero("hammer");
			const auto robot = FindPhosphorIconGlyphOrZero("robot");
			const auto brain = FindPhosphorIconGlyphOrZero("brain");
			const auto layout = FindPhosphorIconGlyphOrZero("layout");
			const auto archiveBox =
				FindPhosphorIconGlyphOrZero("box-arrow-down");
			require(
				ResolveIconGlyph(IconKind::kClient, "acorn") ==
						FindPhosphorIconGlyphOrZero("acorn") &&
					ResolveIconGlyph(IconKind::kClient, "acorn") != char32_t{},
				"full generated icon catalog was not consulted");
			const std::array normalizationCases{
				std::pair{ "DearModdingUI", "dear-modding-ui" },
				std::pair{ "UISettings", "ui-settings" },
				std::pair{ "3D Camera", "3-d-camera" },
				std::pair{ "arrow_counter.clockwise", "arrow-counter-clockwise" },
				std::pair{ "  Power---Armor  ", "power-armor" },
				std::pair{ "3d", "3d" }
			};
			for (const auto& [source, expected] : normalizationCases)
				require(NormalizeIconName(source) == expected,
					"runtime icon normalization drifted");
			require(
				ResolveNamedIconGlyphOrZero("archive-box") == archiveBox,
				"accepted upstream alias did not resolve exactly");
			require(
				ResolveSemanticIconGlyph(
					"hammer",
					"Wrench Mod",
					"General",
					PhosphorGlyph::kQuestion) == hammer,
				"explicit icon did not win over metadata");
			require(
				ResolveSemanticIconGlyph(
					"unknown",
					"Wrench Mod",
					"General",
					PhosphorGlyph::kQuestion) == wrench &&
					ResolveClientIconGlyph({}, "General", "Wrench Mod") ==
						wrench &&
					ResolveInferredIconGlyphOrZero("Wrench Tools") == wrench,
				"primary canonical phrase did not outrank secondary or tags");
			require(
				ResolveInferredIconGlyphOrZero("Power Armor") == robot &&
					ResolveInferredIconGlyphOrZero("repairs") == wrench,
				"domain phrase or generated tag coverage was lost");

			const std::array ambiguousPrimary{
				std::string_view{ "AI / UI" }
			};
			const std::array brainContext{ std::string_view{ "Brain" } };
			const std::array mixedContext{
				std::string_view{ "Cloud Sun" },
				std::string_view{ "Brain" }
			};
			const std::array unrelatedContext{
				std::string_view{ "General" }
			};
			const auto ambiguous = IconResolver::Resolve({
				.primaryMetadata = ambiguousPrimary
			});
			const auto narrowed = IconResolver::Resolve({
				.primaryMetadata = ambiguousPrimary,
				.secondaryMetadata = brainContext
			});
			const auto unrelated = IconResolver::Resolve({
				.primaryMetadata = ambiguousPrimary,
				.secondaryMetadata = unrelatedContext
			});
			const auto narrowedWithUnrelated = IconResolver::Resolve({
				.primaryMetadata = ambiguousPrimary,
				.secondaryMetadata = mixedContext
			});
			require(
				ambiguous.status == IconSelectionStatus::kAmbiguous &&
					narrowed.HasSelection() && narrowed.glyph == brain &&
					narrowedWithUnrelated.HasSelection() &&
					narrowedWithUnrelated.glyph == brain &&
					unrelated.status == IconSelectionStatus::kAmbiguous,
				"ambiguity or secondary narrowing changed");
			const std::array toolsPrimary{ std::string_view{ "Tools" } };
			const auto toolsWithGeneral = IconResolver::Resolve({
				.primaryMetadata = toolsPrimary,
				.secondaryMetadata = unrelatedContext
			});
			require(
				toolsWithGeneral.status == IconSelectionStatus::kAmbiguous,
				"unrelated secondary context replaced ambiguous Tools");
			const std::array firstOrder{
				std::string_view{ "AI" },
				std::string_view{ "UI" }
			};
			const std::array reverseOrder{
				std::string_view{ "UI" },
				std::string_view{ "AI" }
			};
			require(
				IconResolver::Resolve({
					.secondaryMetadata = firstOrder
				}).status == IconSelectionStatus::kAmbiguous &&
					IconResolver::Resolve({
						.secondaryMetadata = reverseOrder
					}).status == IconSelectionStatus::kAmbiguous,
				"peer metadata order changed an ambiguous result");
			require(
				ResolveInferredIconGlyphOrZero("Detail") != brain &&
					ResolveInferredIconGlyphOrZero("Fluid") != layout &&
					ResolveInferredIconGlyphOrZero("X Frobnicator") ==
						char32_t{} &&
					ResolveInferredIconGlyphOrZero("X") ==
						PhosphorGlyph::kX,
				"short authoritative terms matched inside words");

			require(
				ResolveCategoryIconGlyph(
					"Wrench",
					"Wrench",
					"wrench",
					"hammer") == wrench &&
					ResolveCategoryIconGlyph(
						"Wrench",
						"Wrench",
						"wrench",
						"hammer",
						"robot") == robot,
				"category inference inherited the matching client's icon");

			const auto noMatch = ResolveIconSelection(
				"unknown",
				"Unmapped Frobnicator");
			require(
				noMatch.status == IconSelectionStatus::kNoMatch &&
					noMatch.GlyphOr(PhosphorGlyph::kQuestion) ==
						PhosphorGlyph::kQuestion &&
					noMatch.GlyphOr(PhosphorGlyph::kFiles) ==
						PhosphorGlyph::kFiles &&
					noMatch.GlyphOr(PhosphorGlyph::kTerminalWindow) ==
						PhosphorGlyph::kTerminalWindow &&
					noMatch.GlyphOr({}) == char32_t{},
				"surface-specific no-match defaults were not independent");

			const auto invalidRaw = IconResolver::Resolve({
				.explicitGlyph = char32_t{ 0x110000 }
			});
			NavigationClient navigationClient;
			navigationClient.id = "wrench";
			navigationClient.displayName = "Wrench";
			navigationClient.iconName = "hammer";
			navigationClient.iconSelection =
				ResolveIconSelection("hammer", "Wrench");
			NavigationCategory navigationCategory;
			navigationCategory.id = "wrench";
			navigationCategory.displayName = "Wrench";
			navigationCategory.iconSelection =
				ResolveIconSelection({}, "Wrench");
			navigationClient.categories.push_back(navigationCategory);
			require(
				ResolveNavigationClientIconGlyph(navigationClient) == hammer &&
					ResolveNavigationCategoryIconGlyph(
						navigationClient.categories.front()) == wrench,
				"cached navigation selections were not independent");

			NavigationSearchEntry page;
			page.kind = NavigationItemKind::kPage;
			page.iconSelection = noMatch;
			require(
				ResolveNavigationSearchEntryGlyph(page) ==
					PhosphorGlyph::kFiles,
				"page palette miss lost the Files fallback");

			NavigationSearchEntry action;
			action.kind = NavigationItemKind::kAction;
			action.iconSelection =
				ResolveIconSelection("unknown", "Audio Feature");
			require(
				ResolveNavigationSearchEntryGlyph(action) ==
					PhosphorGlyph::kSpeakerHigh,
				"action palette did not use the shared cached match");
			action.iconSelection = noMatch;
			require(
				ResolveNavigationSearchEntryGlyph(action) ==
						PhosphorGlyph::kTerminalWindow &&
					ResolveActionIconGlyph(
						"unknown",
						"Unmapped Frobnicator") == char32_t{},
				"action surfaces lost their distinct no-match defaults");
			require(
				ResolveAutomaticIconGlyph({}, "Wrench", PhosphorGlyph::kQuestion) ==
						wrench &&
					ResolveAutomaticIconGlyph(
						PhosphorGlyph::kSun,
						"Wrench",
						PhosphorGlyph::kQuestion) == PhosphorGlyph::kSun &&
					invalidRaw.status ==
						IconSelectionStatus::kInvalidRawGlyph &&
					invalidRaw.GlyphOr(PhosphorGlyph::kQuestion) ==
						char32_t{ 0x110000 },
				"automatic group or invalid raw-glyph semantics changed");
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
