#include <DearModdingUI/MCM/Keybinds.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "Harness.h"
#include "FakeDiagnosticReporter.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace vmm_tests
{
	namespace
	{
		FakeDiagnosticReporter diagnostics;

		using namespace DearModdingUI::MCM;

		class EmptyValueSource final : public ValueSource
		{
		public:
			[[nodiscard]] bool Supports(SourceFamily) const noexcept override
			{
				return false;
			}

			[[nodiscard]] ValueSnapshot Read(const MappedBinding&) const override
			{
				return MissingValue{};
			}

			[[nodiscard]] uint64_t Refresh(const MappedBinding&) override
			{
				return 0;
			}

			[[nodiscard]] ValueSnapshot Write(
				const MappedBinding&,
				const dmui::SettingValue&) override
			{
				return MissingValue{};
			}
		};

		[[nodiscard]] LoadResult MakeHotkeyConfig()
		{
			return ParseConfig(R"json({
				"modName":"MyMod",
				"displayName":"My Mod",
				"content":[
					{"id":"keyPlain","type":"hotkey","text":"Plain","help":"Plain help"},
					{"id":"keyModified","type":"hotkey","text":"Modified","help":"Modified help"},
					{"id":"keyMissing","type":"hotkey","text":"Missing","help":"Missing help"}
				]
			})json", "keybind-test-config.json");
		}

		[[nodiscard]] dmui::SettingDescriptor& SettingNamed(
			MappedPage& a_page,
			std::string_view a_id)
		{
			for (auto& group : a_page.settings.groups)
				for (auto& setting : group.settings)
					if (setting.id == a_id)
						return setting;
			throw Failure("setting not found");
		}

		[[nodiscard]] MappedRow& RowNamed(
			MappedPage& a_page,
			std::string_view a_id)
		{
			for (auto& row : a_page.rows)
				if (row.id == a_id)
					return row;
			throw Failure("mapped row not found");
		}

		[[nodiscard]] std::string DisplayedValue(
			MappedPage& a_page,
			std::string_view a_id)
		{
			return std::get<std::string>(
				SettingNamed(a_page, a_id).defaultValue);
		}
	}

	void run_mcm_keybind_checks(Runner& runner)
	{
		runner.test("MCM keybind loaders ingest definitions and user bindings", [] {
			const auto root =
				std::filesystem::temp_directory_path() / "dmui-mcm-keybind-loader";
			const auto definitionsPath = root / "Config" / "keybinds.json";
			const auto bindingsPath = root / "Settings" / "Keybinds.json";
			std::filesystem::create_directories(definitionsPath.parent_path());
			std::filesystem::create_directories(bindingsPath.parent_path());
			{
				std::ofstream definitions{ definitionsPath, std::ios::binary };
				definitions << R"({"modName":"PathMod","keybinds":[
					{"id":"keyPlain","desc":"Plain","action":{"type":"SendEvent","event":"Plain"}},
					{"id":"keyModified","desc":"Modified","action":{"type":"RunConsoleCommand","command":"help"}}
				]})";
				std::ofstream bindings{ bindingsPath, std::ios::binary };
				bindings << R"({"version":1,"keybinds":[
					{"keycode":30,"modifiers":0,"modName":"PathMod","id":"keyPlain"},
					{"keycode":37,"modifiers":3,"modName":"PathMod","id":"keyModified"}
				]})";
			}
			const auto definitions = LoadKeybindDefinitions(definitionsPath);
			const auto bindings = LoadUserKeybinds(bindingsPath);
			const auto* modified = bindings.Find("PathMod", "keyModified");
			std::error_code error;
			std::filesystem::remove_all(root, error);
			require(definitions.state == KeybindFileState::kLoaded &&
					definitions.modName == "PathMod" &&
					definitions.Contains("keyPlain") &&
					definitions.Contains("keyModified") &&
					bindings.state == KeybindFileState::kLoaded &&
					modified && modified->keycode == 37 &&
					modified->modifiers == 3,
				"loader paths or parsed keybind content changed");
		});

		runner.test("MCM keybind display resolves keys and modifiers", [] {
			auto result = MakeHotkeyConfig();
			require(result.configuration && result.pages.size() == 1,
				"hotkey fixture did not map");
			const auto definitions = ParseKeybindDefinitions(R"({
				"modName":"MyMod",
				"keybinds":[{"id":"keyPlain"},{"id":"keyModified"},{"id":"keyMissing"}]
			})");
			const auto bindings = ParseUserKeybinds(R"({
				"keybinds":[
					{"keycode":30,"modifiers":0,"modName":"MyMod","id":"keyPlain"},
					{"keycode":37,"modifiers":3,"modName":"MyMod","id":"keyModified"}
				]
			})");
			auto& page = result.pages.front();
			ApplyKeybinds(page, definitions, bindings, diagnostics);
			require(DisplayedValue(page, "keyPlain") == "A" &&
					DisplayedValue(page, "keyModified") == "Ctrl+Shift+K",
				"bound keyboard names or stable modifier order changed");
			require(RowNamed(page, "keyPlain").keybindInertState &&
					RowNamed(page, "keyPlain")
							.keybindInertState->governingReason ==
						InertReason::kNone &&
					SummarizeCompatibility(page).resolvedKeybinds == 2,
				"resolved keybind state or count was lost");
			require(KeyName(255) == "Keycode 255" &&
					KeyName(256) == "Mouse 1" &&
					KeyName(263) == "Mouse 8" &&
					KeyName(264) == "Mouse Wheel Up" &&
					KeyName(265) == "Mouse Wheel Down" &&
					KeyName(266) == "D-Pad Up" &&
					KeyName(281) == "Right Trigger" &&
					KeyName(282) == "Keycode 282",
				"keyboard, mouse, wheel, or gamepad boundaries changed");
			require(FormatKeybind(266, 7) == "Ctrl+Shift+Alt+D-Pad Up",
				"modifier order or gamepad naming changed");
		});

		runner.test("MCM keybind display distinguishes unbound and undeclared", [] {
			auto result = MakeHotkeyConfig();
			auto& page = result.pages.front();
			ApplyKeybinds(
				page,
				ParseKeybindDefinitions(R"({
					"modName":"MyMod",
					"keybinds":[{"id":"keyPlain"},{"id":"keyModified"}]
				})"),
				ParseUserKeybinds(R"({"keybinds":[]})"),
				diagnostics);
			const auto& unbound = *RowNamed(page, "keyPlain").keybindInertState;
			const auto& missing = *RowNamed(page, "keyMissing").keybindInertState;
			require(DisplayedValue(page, "keyPlain") == "Unbound" &&
					unbound.governingReason == InertReason::kKeybindUnbound &&
					Describe(unbound.rowReason).scope == InertReasonScope::kRow,
				"a declared unbound key lost its row-scoped state");
			require(DisplayedValue(page, "keyMissing") == "Can't be bound" &&
					missing.governingReason ==
						InertReason::kKeybindDefinitionMissing &&
					Describe(missing.rowReason).scope == InertReasonScope::kRow,
				"an undeclared key lost its distinct row-scoped state");
			require(SummarizeActionableCompatibility(page).empty(),
				"an optional unbound key created a permanent startup warning");
		});

		runner.test("MCM missing definitions are reported once per page", [] {
			auto result = MakeHotkeyConfig();
			auto& page = result.pages.front();
			ApplyKeybinds(page, {}, {}, diagnostics);
			for (const auto& row : page.rows)
			{
				require(row.keybindInertState &&
						row.keybindInertState->governingReason ==
							InertReason::kKeybindDefinitionsMissing &&
						row.keybindInertState->rowReason == InertReason::kNone,
					"missing definitions produced a per-row explanation");
			}
			EmptyValueSource source;
			BindPage(page, source);
			page.settings.prepareView(page.settings);
			require(page.settings.notes.size() == 1 &&
					page.settings.notes.front().text.find(
						"no MCM keybinds.json") != std::string::npos,
				"missing definitions were not reported once at page scope");
		});

		runner.test("MCM missing user keybind file means unbound", [] {
			const auto missingPath =
				std::filesystem::temp_directory_path() /
				"dmui-mcm-no-user-keybinds.json";
			std::error_code error;
			std::filesystem::remove(missingPath, error);
			const auto bindings = LoadUserKeybinds(missingPath);
			require(bindings.state == KeybindFileState::kMissing,
				"an absent user keybind file was treated as malformed");
			auto result = MakeHotkeyConfig();
			auto& page = result.pages.front();
			ApplyKeybinds(
				page,
				ParseKeybindDefinitions(R"({
					"modName":"MyMod",
					"keybinds":[{"id":"keyPlain"},{"id":"keyModified"},{"id":"keyMissing"}]
				})"),
				bindings,
				diagnostics);
			require(DisplayedValue(page, "keyPlain") == "Unbound" &&
					RowNamed(page, "keyPlain")
							.keybindInertState->governingReason ==
						InertReason::kKeybindUnbound,
				"an absent user file stopped meaning unbound");
		});

		runner.test("MCM malformed keybind JSON remains explicit", [] {
			const auto definitions = ParseKeybindDefinitions("{not json");
			const auto bindings = ParseUserKeybinds(R"({"keybinds":[
				{"keycode":"A","modifiers":0,"modName":"MyMod","id":"keyPlain"}
			]})");
			require(definitions.state == KeybindFileState::kMalformed &&
					bindings.state == KeybindFileState::kMalformed,
				"malformed definition or user JSON was accepted");

			auto invalidDefinitions = MakeHotkeyConfig();
			ApplyKeybinds(
				invalidDefinitions.pages.front(),
				definitions,
				{},
				diagnostics);
			require(DisplayedValue(
						invalidDefinitions.pages.front(),
						"keyPlain") == "Can't be bound" &&
					RowNamed(invalidDefinitions.pages.front(), "keyPlain")
							.keybindInertState->governingReason ==
						InertReason::kKeybindDefinitionsInvalid,
				"malformed definitions did not remain explicit");

			auto invalidBindings = MakeHotkeyConfig();
			ApplyKeybinds(
				invalidBindings.pages.front(),
				ParseKeybindDefinitions(R"({
					"modName":"MyMod",
					"keybinds":[{"id":"keyPlain"},{"id":"keyModified"},{"id":"keyMissing"}]
				})"),
				bindings,
				diagnostics);
			require(DisplayedValue(
						invalidBindings.pages.front(),
						"keyPlain") == "Binding unavailable" &&
					RowNamed(invalidBindings.pages.front(), "keyPlain")
							.keybindInertState->governingReason ==
						InertReason::kKeybindBindingsInvalid,
				"malformed user bindings looked merely unbound");
		});
	}
}
