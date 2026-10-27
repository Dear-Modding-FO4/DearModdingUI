#include "../support/DearModdingUITestSupport.h"
#include "../support/ImGuiTestContext.h"
#include <DearModdingUI/presentation/FontCatalog.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/settings/HostSettingsHealthState.h>
#include <DearModdingUI/settings/HostSettingsView.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/presentation/FontMetrics.h>
#include <Platform/settings/GameColors.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI;
	using namespace support::host;

	void run_host_settings_checks(Runner& runner)
	{
		runner.test("host settings draft reverts and resets safely", [] {
			HostInterfaceSettings changed;
			changed.sidebarLayout = SidebarLayoutKind::DrillDown;
			changed.themePreset = "Stress";
			changed.appearance.iconColorMode = Theme::IconColorMode::kMonochrome;
			changed.appearance.accentColor = HostAccentColor{ 0xD5, 0x5E, 0x00 };
			changed.appearance.windowBackgroundOpacity = 0.80f;
			changed.appearance.paletteBackgroundColor =
				HostPaletteColor{ 0x12, 0x12, 0x12 };
			changed.appearance.paletteBackgroundOpacity = 0.70f;
			changed.appearance.backgroundBlur = false;
			changed.appearance.backgroundBlurStrength = 0.75f;
			changed.appearance.bodyFontFamily = "Atkinson Hyperlegible";
			changed.appearance.monospaceFontFamily = "ImGui";
			changed.uiScale = 1.75f;
			changed.menuToggleKey = "Home";
			auto state = BeginHostSettingsDraft(changed);
			state.draft.appearance.accentColor = HostAccentColor{ 0xE6, 0x9F, 0x00 };
			state.draft.fallSoulsMode = true;
			RevertHostSettingsDraft(state);
			require(state.draft == changed &&
					!HostSettingsDraftDiffers(state),
				"revert did not restore committed preview fields");

			state.draft.appearance.accentColor = HostAccentColor{ 0xE6, 0x9F, 0x00 };
			LeaveHostSettingsDraft(state);
			require(!state.active && state.draft == changed,
				"leaving settings did not discard the draft");

			state = BeginHostSettingsDraft(changed);
			ResetHostSettingsDraft(state);
			require(state.draft == DefaultHostInterfaceSettings(),
				"reset did not populate shipped defaults");
			require(state.committed == changed &&
					HostSettingsDraftDiffers(state),
				"reset committed instead of updating the draft");

			state = BeginHostSettingsDraft(DefaultHostInterfaceSettings());
			state.draft.appearance.accentColor = HostAccentColor{ 0x00, 0x72, 0xB2 };
			CommitHostSettingsSidebarLayout(state, SidebarLayoutKind::DrillDown);
			LeaveHostSettingsDraft(state);
			require(
				state.draft.sidebarLayout == SidebarLayoutKind::DrillDown &&
					!state.draft.appearance.accentColor &&
					!HostSettingsDraftDiffers(state),
				"discarding cosmetic previews also discarded the saved layout");
			state = BeginHostSettingsDraft(state.committed);
			ResetHostSettingsDraft(state);
			CommitHostSettingsSidebarLayout(state, state.draft.sidebarLayout);
			RevertHostSettingsDraft(state);
			require(
				state.committed.sidebarLayout == DEFAULT_SIDEBAR_LAYOUT &&
					state.draft == state.committed,
				"reset and revert disagreed with the immediately saved layout");
		});

		runner.test("host settings persistence round trips every stored value", [] {
			HostInterfaceSettings monochrome;
			monochrome.sidebarLayout = SidebarLayoutKind::TwoPane;
			monochrome.themePreset = "Stress";
			monochrome.appearance.iconColorMode = Theme::IconColorMode::kMonochrome;
			monochrome.appearance.accentColor = HostAccentColor{ 0x00, 0x72, 0xB2 };
			monochrome.appearance.windowBackgroundOpacity = 0.80f;
			monochrome.appearance.paletteBackgroundColor =
				HostPaletteColor{ 0x12, 0x12, 0x12 };
			monochrome.appearance.paletteBackgroundOpacity = 0.70f;
			monochrome.appearance.backgroundBlur = false;
			monochrome.appearance.backgroundBlurStrength = 0.75f;
			monochrome.appearance.bodyFontFamily = "Atkinson Hyperlegible";
			monochrome.appearance.monospaceFontFamily = "ImGui";
			monochrome.uiScale = 1.75f;
			monochrome.menuToggleKey = "F11";
			HostInterfaceSettings minimums;
			minimums.sidebarLayout = SidebarLayoutKind::DrillDown;
			minimums.appearance.iconColorMode = Theme::IconColorMode::kColored;
			minimums.appearance.accentColor = HostAccentColor{ 0xD5, 0x5E, 0x00 };
			minimums.appearance.windowBackgroundOpacity =
				kMinWindowBackgroundOpacity;
			minimums.appearance.paletteBackgroundOpacity =
				kMinPaletteBackgroundOpacity;
			minimums.appearance.backgroundBlur = true;
			minimums.appearance.backgroundBlurStrength =
				kMinBackgroundBlurStrength;
			minimums.appearance.logoColors = LogoColorMode::kAccent;
			minimums.uiScale = Theme::kMinUserScale;
			minimums.menuToggleKey = "Delete";
			minimums.feedbackPlacement = FieldFeedbackPlacement::kUnderControl;
			minimums.fallSoulsMode = true;
			for (const auto& runtime :
				{ DefaultHostInterfaceSettings(), monochrome, minimums })
			{
				const auto persisted = EncodeHostInterfaceSettings(runtime);
				require(
					DecodeHostInterfaceSettings(persisted) == runtime,
					"stored host settings did not round trip");
				require(
					EncodeHostInterfaceSettings(
						DecodeHostInterfaceSettings(persisted)) == persisted,
					"encoded host settings did not round trip");
			}
		});

		runner.test("appearance overrides are written only when set and clear back to the preset", [] {
			const auto root =
				std::filesystem::current_path() / ".Build" / "Tests" / "override-persistence";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			std::filesystem::create_directories(root, error);
			const auto path = root / "DearModdingUI.toml";
			const auto readFile = [&] {
				std::ifstream file(path);
				return std::string{ std::istreambuf_iterator<char>(file), {} };
			};

			auto settings = DefaultHostInterfaceSettings();
			settings.themePreset = "Stress";
			require(PersistHostInterfaceSettings(
						path, EncodeHostInterfaceSettings(settings)).saved,
				"default appearance could not be saved");
			auto text = readFile();
			for (const auto key : { "sMenuAccentColor", "fMenuWindowOpacity",
									"sMenuPaletteBackgroundColor", "fMenuPaletteOpacity",
									"bMenuMonochromeIcons", "bMenuBackgroundBlur",
									"fMenuBackgroundBlurStrength", "sMenuLogoColors",
									"sMenuBodyFontFamily", "sMenuMonospaceFontFamily" })
			{
				require(text.find(key) == std::string::npos,
					std::string{ key } + " was written without an override");
			}
			auto loaded = LoadHostInterfaceSettings(path);
			require(loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
					loaded.settings == settings &&
					loaded.settings.themePreset == "Stress",
				"absent overrides did not load as the preset's values");

			settings.appearance.accentColor = HostAccentColor{ 0x00, 0x72, 0xB2 };
			settings.appearance.backgroundBlur = false;
			settings.appearance.iconColorMode = Theme::IconColorMode::kMonochrome;
			require(PersistHostInterfaceSettings(
						path, EncodeHostInterfaceSettings(settings)).saved,
				"overrides could not be saved");
			text = readFile();
			require(text.find("sMenuAccentColor") != std::string::npos &&
					text.find("bMenuBackgroundBlur") != std::string::npos &&
					text.find("fMenuWindowOpacity") == std::string::npos,
				"only the set overrides should be written");
			loaded = LoadHostInterfaceSettings(path);
			require(loaded.settings == settings,
				"set overrides did not load back");

			settings.appearance.accentColor.reset();
			require(PersistHostInterfaceSettings(
						path, EncodeHostInterfaceSettings(settings)).saved,
				"cleared override could not be saved");
			loaded = LoadHostInterfaceSettings(path);
			require(!loaded.settings.appearance.accentColor &&
					loaded.settings.appearance.backgroundBlur == false,
				"a cleared override came back");

			std::ofstream(path, std::ios::trunc)
				<< "[Additional]\n"
				<< "sMenuAccentColor = \"bad\"\n"
				<< "fMenuWindowOpacity = 0.5\n"
				<< "sMenuThemePreset = \"..\\\\escaped\"\n";
			loaded = LoadHostInterfaceSettings(path);
			require(loaded.disposition == HostSettingsLoadDisposition::kCorrected &&
					!loaded.settings.appearance.accentColor &&
					loaded.settings.appearance.windowBackgroundOpacity == 0.5f &&
					loaded.settings.themePreset == kDefaultThemePreset &&
					loaded.detail.find("sMenuAccentColor") != std::string::npos,
				"a malformed override was not reported and dropped");
			std::filesystem::remove_all(root, error);
		});

		runner.test("icon set maps Phosphor names, reports unknown, and resolves lookups", [] {
			const std::array named{
				Theme::NamedIconGlyph{ "x", 0xE001 },
				Theme::NamedIconGlyph{ "gear", 0xE002 },
				Theme::NamedIconGlyph{ "no-such-icon", 0xE003 },
				Theme::NamedIconGlyph{ "gear", 0xE004 }
			};
			const auto mapped = Theme::MapIconNames(named);
			require(
				mapped.unknownNames == std::vector<std::string>{ "no-such-icon" },
				"unknown icon name was not returned");
			require(
				mapped.glyphs.size() == 2 &&
					mapped.glyphs[0].phosphor < mapped.glyphs[1].phosphor,
				"mapping was not sorted and deduplicated");
			Theme::IconSet icons;
			icons.glyphs = mapped.glyphs;
			require(
				icons.Find(dmui::PhosphorGlyph::kGear) == 0xE004 &&
					icons.Find(dmui::PhosphorGlyph::kX) == 0xE001,
				"mapped icons did not resolve to the last entry");
			require(
				icons.Find(dmui::PhosphorGlyph::kQuestion) == 0,
				"unmapped icon resolved to a glyph");
		});

		runner.test("host settings clamp malformed persisted values and reset", [] {
			PersistedHostInterfaceSettings persisted;
			persisted.accentColor = "#GG00ZZ";
			persisted.windowBackgroundOpacity = -5.0f;
			persisted.paletteBackgroundColor = "#palette";
			persisted.paletteBackgroundOpacity =
				std::numeric_limits<float>::infinity();
			persisted.backgroundBlurStrength =
				std::numeric_limits<float>::infinity();
			persisted.uiScale = 99.0f;
			persisted.bodyFontFamily = "..\\escaped";
			persisted.monospaceFontFamily = "..\\escaped";
			persisted.menuToggleKey = "Hyper";
			persisted.feedbackPlacement = "beside";
			persisted.sidebarLayout = "columns";
			persisted.themePreset = "..\\escaped";
			const auto decoded = DecodeHostInterfaceSettings(persisted);
			const auto& appearance = decoded.appearance;
			require(
				!appearance.accentColor,
				"malformed accent did not fall back to the preset");
			require(
				appearance.windowBackgroundOpacity ==
					kMinWindowBackgroundOpacity,
				"window opacity did not clamp");
			require(
				!appearance.paletteBackgroundColor,
				"malformed palette background did not fall back to the preset");
			require(
				!appearance.paletteBackgroundOpacity,
				"non-finite palette opacity did not fall back to the preset");
			require(
				!appearance.backgroundBlurStrength,
				"non-finite blur strength did not fall back to the preset");
			require(
				decoded.uiScale == Theme::kMaxUserScale,
				"UI scale did not clamp");
			require(
				!appearance.bodyFontFamily && !appearance.monospaceFontFamily,
				"malformed font family did not fall back to the preset");
			require(
				decoded.themePreset == kDefaultThemePreset,
				"malformed theme preset did not fall back");
			require(
				decoded.menuToggleKey ==
					SerializeHotkeyChord(kMenuDefaultToggleChord),
				"malformed toggle key did not fall back");
			require(
				decoded.feedbackPlacement == DEFAULT_FIELD_FEEDBACK_LAYOUT,
				"malformed feedback placement did not fall back");
			require(decoded.sidebarLayout == SidebarLayoutKind::Tree,
				"an unavailable sidebar layout did not fall back to tree");
		});

		runner.test("host settings health distinguishes absent valid and malformed files", [] {
			const auto root =
				std::filesystem::current_path() /
				".Build" /
				"Tests" /
				"health-config-fixture";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			std::filesystem::create_directories(root, error);
			const auto path = root / "DearModdingUI.toml";

			auto loaded = LoadHostInterfaceSettings(path);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kMissing &&
					!loaded.settings.fallSoulsMode &&
					loaded.detail.find("Using defaults") != std::string::npos,
				"an absent optional host config was not healthy");

			std::ofstream(path)
				<< "[Additional]\n"
				<< "sMenuToggleKey = \"F11\"\n"
				<< "sMenuSidebarLayout = \"tree\"\n";
			loaded = LoadHostInterfaceSettings(path);
			const auto toggle = ParseMenuToggleChord(loaded.settings.menuToggleKey);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
					loaded.settings.menuToggleKey == "F11" &&
					!loaded.settings.fallSoulsMode &&
					loaded.settings.menuToggleGamepad == "PadBack+PadLB+PadRB" &&
					toggle.recognized && toggle.chord == HotkeyChord{ 0x57, 0 },
				"an old valid host config did not retain the default FallSouls mode");

			std::ofstream(path, std::ios::trunc)
				<< "[Additional]\n"
				<< "sMenuToggleKey = \"ctrl+f5\"\n"
				<< "sMenuToggleGamepad = \"none\"\n";
			loaded = LoadHostInterfaceSettings(path);
			const auto chord = ParseMenuToggleChord(loaded.settings.menuToggleKey);
			require(loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
					loaded.settings.menuToggleKey == "Ctrl+F5" &&
					loaded.settings.menuToggleGamepad == "none" &&
					chord.recognized &&
					chord.chord == HotkeyChord{ 0x3F, kHotkeyModifierControl },
				"the toggle chord lost its modifiers or canonical spelling");
			require(DecideMenuToggle(false, true) &&
					!DecideMenuToggle(true, true) &&
					DecideMenuToggle(true, false),
				"toggle visibility decisions did not restore a hidden drawing backend");
			for (const auto invalid : { "Ctrl+PadA", "F11", "PadLB+PadLB", "Hyper" })
			{
				std::ofstream(path, std::ios::trunc)
					<< "[Additional]\nsMenuToggleGamepad = \"" << invalid << "\"\n";
				loaded = LoadHostInterfaceSettings(path);
				require(loaded.disposition == HostSettingsLoadDisposition::kCorrected &&
						loaded.settings.menuToggleGamepad == "PadBack+PadLB+PadRB" &&
						loaded.detail.find("sMenuToggleGamepad") != std::string::npos,
					"invalid gamepad toggle lost fallback or diagnostics");
			}

			std::ofstream(path, std::ios::trunc)
				<< "[Additional\n";
			loaded = LoadHostInterfaceSettings(path);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kFailed &&
					loaded.detail.find("correct or remove") !=
						std::string::npos,
				"a malformed host config did not retain actionable failure");

			loaded = LoadHostInterfaceSettings(root);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kFailed,
				"an unreadable host config path was treated as loaded");
			std::filesystem::remove_all(root, error);
		});

		runner.test("host settings health reports actual accepted fallbacks", [] {
			const auto root =
				std::filesystem::current_path() /
				".Build" /
				"Tests" /
				"health-config-corrections";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			std::filesystem::create_directories(root, error);
			const auto path = root / "DearModdingUI.toml";
			std::ofstream(path)
				<< "[Additional]\n"
				<< "sMenuToggleKey = \"Hyper\"\n"
				<< "sMenuSidebarLayout = \"columns\"\n"
				<< "fMenuUiScale = 99.0\n"
				<< "sMenuAccentColor = \"bad\"\n"
				<< "sMenuBodyFontFamily = \"..\\\\escaped\"\n";

			const auto loaded = LoadHostInterfaceSettings(path);
			HostSettingsHealthState state;
			state.RecordLoad(loaded);
			auto observation = state.Observation();
			require(
				loaded.disposition == HostSettingsLoadDisposition::kCorrected &&
					loaded.settings.menuToggleKey == "End" &&
					observation.state == HealthState::kDegraded &&
					observation.reason.find(
						"sMenuToggleKey \"Hyper\" used \"End\"") !=
						std::string::npos,
				"configuration health reported a stale toggle fallback");

			state.RecordSaveFailure("access denied");
			observation = state.Observation();
			require(
				observation.state == HealthState::kDegraded &&
					observation.reason.find("access denied") !=
						std::string::npos &&
					observation.reason.find("sMenuToggleKey") !=
						std::string::npos,
				"a save failure erased the active load correction");

			state.RecordSaveSuccess(path.string());
			observation = state.Observation();
			require(
				observation.state == HealthState::kReady &&
					observation.reason.find("Saved accepted settings") !=
						std::string::npos,
				"a successful write did not resolve persisted configuration health");

			std::ofstream(path, std::ios::trunc)
				<< "[Additional]\n"
				<< "sMenuToggleKey = \"none\"\n";
			const auto unbound = LoadHostInterfaceSettings(path);
			state.RecordLoad(unbound);
			require(unbound.disposition == HostSettingsLoadDisposition::kCorrected &&
					unbound.settings.menuToggleKey == "End" &&
					state.Observation().reason.find(
						"sMenuToggleKey \"none\" used \"End\"") != std::string::npos,
				"none disabled the menu toggle instead of reporting the End fallback");
			std::filesystem::remove_all(root, error);
		});

		runner.test("settings saves round trip through normal replacement and the error 17 fallback", [] {
			const auto root =
				std::filesystem::current_path() /
				".Build" /
				"Tests" /
				"settings-persistence";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			std::filesystem::create_directories(root, error);
			const auto path = root / "DearModdingUI.toml";
			std::ofstream(path) << "old settings";

			auto runtime = DefaultHostInterfaceSettings();
			runtime.menuToggleKey = "ctrl+f5";
			auto settings = EncodeHostInterfaceSettings(runtime);
			require(settings.menuToggleKey == "Ctrl+F5",
				"settings encoding did not preserve the canonical toggle chord");
			settings.sidebarLayout = "twopane";
			settings.feedbackPlacement = "strip";
			settings.fallSoulsMode = true;
			settings.hotkeys.emplace("example.action", "Ctrl+H");
			settings.gamepadHotkeys = { { "example.action", "PadA+PadLB" }, { "removed.action", "none" } };
			settings.menuToggleGamepad = "none";
			const auto saved = PersistHostInterfaceSettings(path, settings);
			require(saved.saved && !saved.usedCrossVolumeFallback,
				"a same-volume settings replacement did not succeed normally");
			require(!std::filesystem::exists(path.string() + ".tmp"),
				"a successful settings replacement retained its temporary file");

			const auto loaded = LoadHostInterfaceSettings(path);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
					loaded.settings ==
						DecodeHostInterfaceSettings(settings) &&
					loaded.hotkeys == settings.hotkeys &&
					loaded.gamepadHotkeys == settings.gamepadHotkeys,
				"persisted host settings were not loadable through production parsing");

			settings.menuToggleKey = "Insert";
			settings.fallSoulsMode = false;
			SettingsMoveProbe probe;
			probe.firstError = ERROR_NOT_SAME_DEVICE;
			probe.performSecondMove = true;
			const auto retried = PersistHostInterfaceSettings(
				path,
				settings,
				{ &probe, &ProbeSettingsMove });
			require(
				retried.saved &&
					retried.usedCrossVolumeFallback &&
					probe.calls == 2 &&
					(probe.flags[0] & MOVEFILE_COPY_ALLOWED) == 0 &&
					(probe.flags[1] & MOVEFILE_COPY_ALLOWED) != 0,
				"ERROR_NOT_SAME_DEVICE did not trigger the scoped copy fallback");
			const auto fallbackLoaded = LoadHostInterfaceSettings(path);
			require(
				fallbackLoaded.settings ==
						DecodeHostInterfaceSettings(settings) &&
					fallbackLoaded.hotkeys == settings.hotkeys &&
					fallbackLoaded.gamepadHotkeys == settings.gamepadHotkeys,
				"the cross-volume fallback did not install the serialized settings");
			require(!std::filesystem::exists(path.string() + ".tmp"),
				"the copy fallback retained its temporary file");
			std::filesystem::remove_all(root, error);
		});

		runner.test("failed settings saves preserve the configuration and explain the actual error", [] {
			const auto root =
				std::filesystem::current_path() /
				".Build" /
				"Tests" /
				"settings-save-failure";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			std::filesystem::create_directories(root, error);
			const auto path = root / "DearModdingUI.toml";
			std::ofstream(path)
				<< "[Additional]\n"
				<< "sMenuToggleKey = \"Home\"\n"
				<< "bMenuFallSoulsMode = true\n";
			auto temporary = path;
			temporary += L".tmp";

			PersistedHostInterfaceSettings settings;
			settings.menuToggleKey = "Insert";
			for (const auto firstError : { ERROR_ACCESS_DENIED, ERROR_NOT_SAME_DEVICE })
			{
				SettingsMoveProbe probe;
				probe.firstError = firstError;
				const auto saved = PersistHostInterfaceSettings(
					path, settings, { &probe, &ProbeSettingsMove });
				const auto explanation = DescribeWindowsError(ERROR_ACCESS_DENIED);
				require(!saved.saved && saved.nativeError == ERROR_ACCESS_DENIED &&
							probe.calls == (firstError == ERROR_NOT_SAME_DEVICE ? 2 : 1) &&
							(probe.flags[0] & MOVEFILE_COPY_ALLOWED) == 0 &&
							!std::filesystem::exists(temporary),
					"failed replacement used an unrelated fallback or left temporary data");
				require(explanation != "No system explanation is available" &&
							saved.detail.find(explanation) != std::string::npos &&
							saved.detail.find("Windows error 5") != std::string::npos,
					"replacement failure omitted its readable system explanation");
				if (firstError == ERROR_NOT_SAME_DEVICE)
				{
					require((probe.flags[1] & MOVEFILE_COPY_ALLOWED) != 0 &&
								saved.detail.find(DescribeWindowsError(ERROR_NOT_SAME_DEVICE)) != std::string::npos,
						"failed cross-volume retry lost the original error 17 explanation");
				}
				const auto loaded = LoadHostInterfaceSettings(path);
				require(loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
							loaded.settings.menuToggleKey == "Home" &&
							loaded.settings.fallSoulsMode,
					"failed replacement changed the stored toggle key or FallSouls mode");
			}

			std::filesystem::create_directory(temporary);
			const auto saved = PersistHostInterfaceSettings(path, settings);
			const auto explanation = DescribeWindowsError(saved.nativeError);
			const auto loaded = LoadHostInterfaceSettings(path);
			require(
				!saved.saved &&
					saved.nativeError != ERROR_SUCCESS &&
					saved.detail.find("Opening temporary settings file") !=
						std::string::npos &&
					explanation != "No system explanation is available" &&
					saved.detail.find(explanation) != std::string::npos &&
					loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
					loaded.settings.menuToggleKey == "Home" &&
					loaded.settings.fallSoulsMode,
				"a real temporary open failure changed the current TOML or omitted its system explanation");
			std::filesystem::remove_all(root, error);
		});

		runner.test("font families enumerate faces and resolve variants", [] {
			struct RemoveTree
			{
				std::filesystem::path path;
				~RemoveTree()
				{
					std::error_code error;
					std::filesystem::remove_all(path, error);
				}
			};

			const auto root =
				std::filesystem::temp_directory_path() /
				"AddictolDearModdingUIFontCatalog";
			std::error_code error;
			std::filesystem::remove_all(root, error);
			const RemoveTree cleanup{ root };
			std::filesystem::create_directories(root / "Jost", error);
			std::filesystem::create_directories(
				root / "Atkinson Hyperlegible", error);
			std::filesystem::create_directories(root / "Phosphor", error);
			std::ofstream(root / "Jost" / "Jost-Regular.ttf").put('\0');
			std::ofstream(root / "Jost" / "Jost-SemiBold.ttf").put('\0');
			// Sorts first but names no face, so it must not claim Regular.
			std::ofstream(root / "Jost" / "Jost-Book.ttf").put('\0');
			std::ofstream(
				root /
				"Atkinson Hyperlegible" /
				"AtkinsonHyperlegible-BoldItalic.ttf").put('\0');
			std::ofstream(
				root /
				"Atkinson Hyperlegible" /
				"AtkinsonHyperlegible-Bold.ttf").put('\0');
			std::ofstream(
				root /
				"Atkinson Hyperlegible" /
				"AtkinsonHyperlegible-Regular.ttf").put('\0');
			std::ofstream(
				root /
				"Phosphor" /
				"Phosphor-Fill.ttf").put('\0');

			const auto families = FontCatalog::Enumerate(root);
			require(families.size() == 2,
				"font family folders were not enumerated safely");
			const auto* atkinson = FontCatalog::Resolve(
				"atkinson hyperlegible", families, "Jost");
			require(
				atkinson && atkinson->name == "Atkinson Hyperlegible" &&
					atkinson->faces.size() == 3,
				"font family faces were not parsed from file names");
			const auto ends = [](const FontCatalog::ResolvedFace& a_face,
								 std::string_view a_suffix) {
				return a_face.face && a_face.face->file.ends_with(a_suffix);
			};
			using FontCatalog::FontWeight;
			const auto boldItalic = FontCatalog::ResolveFace(
				*atkinson, FontWeight::kBold, true);
			require(
				boldItalic.exact && ends(boldItalic, "-BoldItalic.ttf"),
				"an exact face was not chosen");
			const auto italic = FontCatalog::ResolveFace(
				*atkinson, FontWeight::kRegular, true);
			require(
				!italic.exact && ends(italic, "-BoldItalic.ttf"),
				"a missing italic did not use the nearest italic weight");
			const auto heavy = FontCatalog::ResolveFace(
				*atkinson, FontWeight::kBlack, false);
			require(
				!heavy.exact && ends(heavy, "-Bold.ttf"),
				"a missing black did not use the nearest upright weight");
			const auto* jost = FontCatalog::Find("Jost", families);
			require(jost, "Jost was not enumerated");
			const auto medium = FontCatalog::ResolveFace(
				*jost, FontWeight::kMedium, false);
			require(
				ends(medium, "Jost-SemiBold.ttf"),
				"a weight tie did not prefer the heavier face from Medium up");
			const auto light = FontCatalog::ResolveFace(
				*jost, FontWeight::kLight, false);
			require(
				ends(light, "Jost-Regular.ttf"),
				"a light request did not choose the nearest weight");
			const auto regular = FontCatalog::ResolveFace(
				*jost, FontWeight::kRegular, false);
			require(
				regular.exact && ends(regular, "Jost-Regular.ttf"),
				"an unnamed file claimed the explicit Regular face");
			const auto* fallback = FontCatalog::Resolve(
				"Missing Family", families, "Jost");
			require(fallback && fallback->name == "Jost",
				"missing font family did not fall back to Jost");
		});
	}
}
