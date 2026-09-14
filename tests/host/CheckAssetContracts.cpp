#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/navigation/Navigation.h>
#include <DearModdingUI/SettingsActions.h>
#include <DearModdingUI/presentation/Theme.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

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
			const auto acorn = FindPhosphorIconGlyphOrZero("acorn");
			const auto city = FindPhosphorIconGlyphOrZero("city");
			const auto files = FindPhosphorIconGlyphOrZero("files");
			const auto watch = FindPhosphorIconGlyphOrZero("watch");
			const auto archiveBox =
				FindPhosphorIconGlyphOrZero("box-arrow-down");
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
				ResolveClientIconGlyph({}, "General", "Wrench Mod") ==
						wrench &&
					ResolveInferredIconGlyphOrZero("Wrench Tools") == wrench,
				"primary canonical phrase did not outrank secondary or tags");

			const std::array peerPrimary{
				std::string_view{ "Wrench Hammer" }
			};
			const auto peerGlyph = (std::min)(wrench, hammer);
			const std::array competingSecondary{ std::string_view{ "Acorn" } };
			const auto withCompetingSecondary = IconResolver::Resolve({
				.primaryMetadata = peerPrimary,
				.secondaryMetadata = competingSecondary
			});
			require(
				withCompetingSecondary.HasSelection() &&
					withCompetingSecondary.glyph == peerGlyph,
				"primary peer matches were not selected deterministically");
			const std::array firstOrder{
				std::string_view{ "Wrench" },
				std::string_view{ "Hammer" }
			};
			const std::array reverseOrder{
				std::string_view{ "Hammer" },
				std::string_view{ "Wrench" }
			};
			require(
				IconResolver::Resolve({
					.secondaryMetadata = firstOrder
				}).glyph == peerGlyph &&
					IconResolver::Resolve({
						.secondaryMetadata = reverseOrder
					}).glyph == peerGlyph,
				"peer metadata order or equal-rank canonical selection was unstable");
			require(
				ResolveInferredIconGlyphOrZero("diagnostics") ==
					FindPhosphorIconGlyphOrZero("stethoscope"),
				"domain vocabulary was not consulted");
			require(
				ResolveInferredIconGlyphOrZero("3d") ==
					FindPhosphorIconGlyphOrZero("cube"),
				"tag-only metadata did not select its lowest matching glyph");
			require(
				ResolveInferredIconGlyphOrZero("Recent Address Book") ==
					FindPhosphorIconGlyphOrZero("address-book"),
				"longest canonical phrase did not outrank a shorter match");

			const std::array pluralCases{
				std::pair{ "Acorns", acorn },
				std::pair{ "Watches", watch },
				std::pair{ "Cities", city }
			};
			for (const auto& [label, expected] : pluralCases)
				require(
					ResolveInferredIconGlyphOrZero(label) == expected,
					"regular plural metadata did not use its known word form");
			const std::array pluralPrimary{ std::string_view{ "Acorns" } };
			const std::array canonicalSecondary{ std::string_view{ "Wrench" } };
			const std::array mixedPrimary{
				std::string_view{ "Acorns" },
				std::string_view{ "Wrench" }
			};
			require(
				ResolveNamedIconGlyphOrZero("Acorns") == char32_t{} &&
					ResolveInferredIconGlyphOrZero("Files") == files &&
					IconResolver::Resolve({
						.primaryMetadata = mixedPrimary
					}).glyph == wrench &&
					IconResolver::Resolve({
						.primaryMetadata = pluralPrimary,
						.secondaryMetadata = canonicalSecondary
					}).glyph == acorn,
				"explicit lookup, direct matches, or plural precedence changed");
			require(
				ResolveInferredIconGlyphOrZero("Census Focus Class") ==
						char32_t{} &&
					ResolveInferredIconGlyphOrZero("Acornucopia") ==
						char32_t{} &&
					ResolveInferredIconGlyphOrZero("X Frobnicator") ==
						char32_t{} &&
					ResolveInferredIconGlyphOrZero("X") ==
						PhosphorGlyph::kX,
				"singular endings or short terms produced a false match");

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
						"acorn") == acorn,
				"category inference inherited the matching client's icon");

			const auto noMatch = ResolveIconSelection(
				"unknown",
				"Unmapped Frobnicator");
			require(
				noMatch.status == IconSelectionStatus::kNoMatch &&
					noMatch.GlyphOr(PhosphorGlyph::kQuestion) ==
						PhosphorGlyph::kQuestion,
				"unmatched metadata lost its surface fallback");

			const auto invalidRaw = IconResolver::Resolve({
				.explicitGlyph = char32_t{ 0x110000 }
			});
			const auto zeroRaw = IconResolver::Resolve({
				.explicitGlyph = char32_t{}
			});
			NavigationSearchEntry page;
			page.kind = NavigationItemKind::kPage;
			page.iconSelection = noMatch;
			require(
				ResolveNavigationSearchEntryGlyph(page) ==
					PhosphorGlyph::kFiles,
				"page palette miss lost the Files fallback");

			NavigationSearchEntry action;
			action.kind = NavigationItemKind::kAction;
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
						char32_t{ 0x110000 } &&
					zeroRaw.status ==
						IconSelectionStatus::kInvalidRawGlyph &&
					zeroRaw.GlyphOr(PhosphorGlyph::kQuestion) ==
						char32_t{},
				"automatic group or invalid raw-glyph semantics changed");
			const auto icon = ResolveSettingsActionButtonPresentation(
				SettingsAction::kReset, true);
			const auto fallback = ResolveSettingsActionButtonPresentation(
				SettingsAction::kReset, false);
			require(
				icon.glyph == PhosphorGlyph::kArrowsClockwise &&
					!icon.useTextFallback &&
					fallback.glyph == char32_t{} && fallback.useTextFallback,
				"missing settings glyph did not select text fallback");
		});

		runner.test("raw icon glyph validation rejects truncating code points", [] {
			require(
				IsRepresentableIconGlyph<ImWchar>(PhosphorGlyph::kSun) &&
					!IsRepresentableIconGlyph<ImWchar>(char32_t{}) &&
					!IsRepresentableIconGlyph<ImWchar>(
						char32_t{ 0x1E472 }),
				"raw glyph validation allowed zero, invalid, or truncating values");
		});

		runner.test("host colors preserve bytes and select icon tint", [] {
			constexpr HostAccentColor color{ 0x00, 0x80, 0xFF };
			const auto accent = HostAccentToImVec4(color);
			require(
				SameColor(accent, { 0.0f, 128.0f / 255.0f, 1.0f, 1.0f }) &&
					HostAccentFromImVec4(accent) == color,
				"color editor conversion changed a stored component");
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
