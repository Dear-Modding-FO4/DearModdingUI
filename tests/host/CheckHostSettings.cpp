#include "../support/DearModdingUITestSupport.h"
#include <DearModdingUI/presentation/FontCatalog.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/settings/HostSettingsHealthState.h>
#include <DearModdingUI/settings/HostSettingsView.h>
#include <DearModdingUI/SettingsActions.h>
#include <DearModdingUI/controls/ChromeGeometry.h>
#include <DearModdingUI/presentation/Theme.h>
#include <DearModdingUI/ThemeDefaults.h>
#include <DearModdingUI/presentation/TypographyHealth.h>
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
		runner.test("settings action rows keep fixed non-overlapping geometry", [] {
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
				const auto buttonPadding = 2.0f * test.uiScale;
				const auto framePadding = 8.0f * test.uiScale;
				const auto spacing = 4.0f * test.uiScale;
				const auto buttonExtent = TitleBarButtonExtent(
					fontSize, buttonPadding);
				const std::array widths{
					ActionButtonWidth(true, 0.0f, buttonExtent, framePadding),
					ActionButtonWidth(true, 0.0f, buttonExtent, framePadding),
					ActionButtonWidth(true, 0.0f, buttonExtent, framePadding)
				};
				const auto cleanWidthSum =
					ResolveSettingsActionButtonWidthSum(widths, false, 0);
				const auto dirtyWidthSum =
					ResolveSettingsActionButtonWidthSum(widths, true, 7);
				require(cleanWidthSum == dirtyWidthSum,
					"draft state or pending count changed action-row extent");
				const auto clean = ResolveHostSettingsTitleRowLayout(
					100.0f,
					1900.0f,
					cleanWidthSum,
					widths.size(),
					buttonExtent,
					spacing);
				const auto dirty = ResolveHostSettingsTitleRowLayout(
					100.0f,
					1900.0f,
					dirtyWidthSum,
					widths.size(),
					buttonExtent,
					spacing);
				const auto cleanPage = ResolvePageActionRowLayout(
					100.0f,
					1900.0f,
					cleanWidthSum,
					widths.size(),
					spacing);
				const auto dirtyPage = ResolvePageActionRowLayout(
					100.0f,
					1900.0f,
					dirtyWidthSum,
					widths.size(),
					spacing);
				const auto priorClose = ResolveTrailingControlLayout(
					100.0f, 1900.0f, buttonExtent, spacing);
				require(
					clean.titleMaxX <= clean.actionsMinX &&
						clean.actionsMaxX + spacing == clean.closeMinX &&
						clean.closeMinX == priorClose.controlMinX &&
						clean.closeMaxX == priorClose.controlMaxX,
					"settings title controls overlapped");

				auto position = clean.actionsMinX;
				for (const auto width : widths)
				{
					require(position + width <= clean.actionsMaxX,
						"settings action exceeded its reserved strip");
					position += width + spacing;
				}
				require(position - spacing == clean.actionsMaxX,
					"settings action spacing changed");
				require(
					clean.reservedWidth == dirty.reservedWidth &&
						clean.actionsMinX == dirty.actionsMinX &&
						clean.closeMinX == dirty.closeMinX,
					"dirty state changed settings title geometry");
				require(
					cleanPage.reservedWidth == dirtyPage.reservedWidth &&
						cleanPage.actionsMinX == dirtyPage.actionsMinX,
					"pending count changed Addictol settings action geometry");
			}
		});

		runner.test("sidebar layout commits outside the discardable settings preview", [] {
			auto state = BeginHostSettingsDraft(
				DefaultHostInterfaceSettings());
			state.draft.accentColor = { 0x00, 0x72, 0xB2 };
			CommitHostSettingsSidebarLayout(
				state,
				SidebarLayoutKind::DrillDown);
			require(
				state.committed.sidebarLayout == SidebarLayoutKind::DrillDown &&
					state.draft.sidebarLayout == SidebarLayoutKind::DrillDown,
				"immediate layout commit did not update both settings baselines");
			LeaveHostSettingsDraft(state);
			require(
				!state.active &&
					state.draft.sidebarLayout == SidebarLayoutKind::DrillDown &&
					state.draft.accentColor ==
						DefaultHostInterfaceSettings().accentColor &&
					!HostSettingsDraftDiffers(state),
				"discarding cosmetic previews also discarded the saved layout");

			state = BeginHostSettingsDraft(state.committed);
			ResetHostSettingsDraft(state);
			CommitHostSettingsSidebarLayout(
				state,
				state.draft.sidebarLayout);
			RevertHostSettingsDraft(state);
			require(
				state.committed.sidebarLayout == DEFAULT_SIDEBAR_LAYOUT &&
					state.draft == state.committed,
				"reset and revert disagreed with the immediately saved layout");
		});

		runner.test("host settings draft reverts and resets safely", [] {
			const HostInterfaceSettings changed{
				Theme::IconColorMode::kMonochrome,
				SidebarLayoutKind::DrillDown,
				{ 0xD5, 0x5E, 0x00 },
				0.80f,
				{ 0x12, 0x12, 0x12 },
				0.70f,
				false,
				0.75f,
				1.75f,
				"Atkinson Hyperlegible",
				"Home"
			};
			auto state = BeginHostSettingsDraft(changed);
			state.draft.accentColor = { 0xE6, 0x9F, 0x00 };
			RevertHostSettingsDraft(state);
			require(state.draft == changed &&
					!HostSettingsDraftDiffers(state),
				"revert did not restore committed preview fields");

			state.draft.accentColor = { 0xE6, 0x9F, 0x00 };
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
		});

		runner.test("host settings previews separate appearance from typography", [] {
			const auto committed = DefaultHostInterfaceSettings();
			auto draft = committed;
			for (const bool changeFont : { false, true })
			{
				draft = committed;
				if (changeFont)
					draft.bodyFontFamily = "Atkinson Hyperlegible";
				else
					draft.uiScale = 1.25f;
				require(PreviewHostInterfaceSettings(draft) ==
						PreviewHostInterfaceSettings(committed),
					"typography settings leaked into the live preview");
			}

			const auto checkAppearance = [&committed](const auto& appearance) {
				require(PreviewHostInterfaceSettings(appearance) !=
							PreviewHostInterfaceSettings(committed),
					"appearance was omitted from preview");
			};
			draft = committed;
			draft.accentColor = { 0x00, 0x72, 0xB2 };
			checkAppearance(draft);
			draft = committed;
			draft.paletteBackgroundColor = { 0x12, 0x12, 0x12 };
			draft.paletteBackgroundOpacity = 0.70f;
			checkAppearance(draft);
			draft = committed;
			draft.sidebarLayout = SidebarLayoutKind::TwoPane;
			checkAppearance(draft);
		});

		runner.test("host settings persistence round trips every stored value", [] {
			std::array settings{
				DefaultHostInterfaceSettings(),
				HostInterfaceSettings{
					Theme::IconColorMode::kMonochrome,
					SidebarLayoutKind::TwoPane,
					{ 0x00, 0x72, 0xB2 },
					0.80f,
					{ 0x12, 0x12, 0x12 },
					0.70f,
					false,
					0.75f,
					1.75f,
					"Atkinson Hyperlegible"
				},
				HostInterfaceSettings{
					Theme::IconColorMode::kColored,
					SidebarLayoutKind::DrillDown,
					{ 0xD5, 0x5E, 0x00 },
					kMinWindowBackgroundOpacity,
					{ 0x02, 0x02, 0x02 },
					kMinPaletteBackgroundOpacity,
					true,
					kMinBackgroundBlurStrength,
					Theme::kMinUserScale,
					"Jost",
					"Delete"
				}
			};
			for (const auto& runtime : settings)
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
			persisted.menuToggleKey = "PageUp";
			const auto decoded = DecodeHostInterfaceSettings(persisted);
			require(
				decoded.accentColor == kDefaultHostAccentColor,
				"malformed accent did not fall back");
			require(
				decoded.windowBackgroundOpacity ==
					kMinWindowBackgroundOpacity,
				"window opacity did not clamp");
			require(
				decoded.paletteBackgroundColor ==
					kDefaultPaletteBackgroundColor,
				"malformed palette background did not fall back");
			require(
				decoded.paletteBackgroundOpacity ==
					kDefaultPaletteBackgroundOpacity,
				"non-finite palette opacity did not fall back");
			require(
				decoded.backgroundBlurStrength ==
					kDefaultBackgroundBlurStrength,
				"non-finite blur strength did not fall back");
			require(
				decoded.uiScale == Theme::kMaxUserScale,
				"UI scale did not clamp");
			require(
				decoded.bodyFontFamily == kDefaultBodyFontFamily,
				"malformed font family did not fall back");
			require(
				decoded.menuToggleKey ==
					MenuToggleKeyName(kMenuDefaultToggleKey),
				"malformed toggle key did not fall back");
			require(
				DefaultHostInterfaceSettings() == HostInterfaceSettings{},
				"reset did not restore shipped defaults");
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
					loaded.detail.find("Using defaults") != std::string::npos,
				"an absent optional host config was not healthy");

			std::ofstream(path)
				<< "[Additional]\n"
				<< "sMenuToggleKey = \"End\"\n"
				<< "sMenuSidebarLayout = \"tree\"\n";
			loaded = LoadHostInterfaceSettings(path);
			require(
				loaded.disposition == HostSettingsLoadDisposition::kLoaded &&
					loaded.settings.menuToggleKey == "End",
				"a valid host config did not report loaded");

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
				<< "sMenuToggleKey = \"PageUp\"\n"
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
						"sMenuToggleKey \"PageUp\" used \"End\"") !=
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

			PersistedHostInterfaceSettings settings;
			settings.menuToggleKey = "Home";
			settings.sidebarLayout = "twopane";
			settings.hotkeys.emplace("example.action", "Ctrl+H");
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
					loaded.hotkeys == settings.hotkeys,
				"persisted host settings were not loadable through production parsing");

			settings.menuToggleKey = "Insert";
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
					fallbackLoaded.hotkeys == settings.hotkeys,
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
				<< "sMenuToggleKey = \"Home\"\n";
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
							loaded.settings.menuToggleKey == "Home",
					"failed replacement changed the stored toggle key");
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
					loaded.settings.menuToggleKey == "Home",
				"a real temporary open failure changed the current TOML or omitted its system explanation");
			std::filesystem::remove_all(root, error);
		});

		runner.test("typography health distinguishes requested fallback and failure", [] {
			Theme::TypographyLoadOutcome ready;
			ready.requestedFamily = "Jost";
			ready.effectiveFamily = "Jost";
			ready.requestedFamilyFound = true;
			ready.requestedBodyLoaded = true;
			ready.rolesLoaded.fill(true);
			ready.iconsLoaded = true;
			ready.usableAtlas = true;
			auto observation = Theme::ClassifyTypographyHealth(ready);
			require(
				observation.state == HealthState::kReady &&
					observation.reason.find("Phosphor") != std::string::npos,
				"complete typography did not report ready");

			auto fallback = ready;
			fallback.requestedFamily = "Missing Family";
			fallback.requestedFamilyFound = false;
			fallback.rolesLoaded[
				static_cast<size_t>(Theme::FontRole::kHeading)] = false;
			fallback.iconsLoaded = false;
			observation = Theme::ClassifyTypographyHealth(fallback);
			require(
				observation.state == HealthState::kDegraded &&
					observation.reason.find("Missing Family") !=
						std::string::npos &&
					observation.reason.find("heading") !=
						std::string::npos &&
					observation.reason.find("text-only") !=
						std::string::npos,
				"typography fallback health lost its concrete causes");

			fallback.emergencyFontUsed = true;
			fallback.effectiveFamily = "Built-in fallback";
			observation = Theme::ClassifyTypographyHealth(fallback);
			require(
				observation.state == HealthState::kDegraded &&
					observation.reason.find("emergency") !=
						std::string::npos,
				"usable emergency typography was reported as failed");

			fallback.usableAtlas = false;
			fallback.emergencyFontUsed = false;
			observation = Theme::ClassifyTypographyHealth(fallback);
			require(
				observation.state == HealthState::kFailed &&
					observation.reason.find("font atlas") !=
						std::string::npos,
				"missing typography capability was not failed");
		});

		runner.test("font families enumerate regular faces and fall back", [] {
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
				atkinson &&
					atkinson->name == "Atkinson Hyperlegible" &&
					atkinson->regularFile.ends_with(
						"AtkinsonHyperlegible-Regular.ttf"),
				"font family did not choose its regular face");
			const auto* fallback = FontCatalog::Resolve(
				"Missing Family", families, "Jost");
			require(fallback && fallback->name == "Jost",
				"missing font family did not fall back to Jost");
		});

	}
}
