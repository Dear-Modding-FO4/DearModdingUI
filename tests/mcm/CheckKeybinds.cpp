#include <DearModdingUI/MCM/Keybinds.h>

#include "../Harness.h"
#include "../FakeDiagnosticReporter.h"

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

			const auto missingPath =
				std::filesystem::temp_directory_path() /
				"dmui-mcm-no-user-keybinds.json";
			std::error_code error;
			std::filesystem::remove(missingPath, error);
			const auto absent = LoadUserKeybinds(missingPath);
			require(absent.state == KeybindFileState::kMissing,
				"an absent user keybind file was treated as malformed");
			auto unbound = MakeHotkeyConfig();
			ApplyKeybinds(
				unbound.pages.front(),
				ParseKeybindDefinitions(R"({
					"modName":"MyMod",
					"keybinds":[{"id":"keyPlain"},{"id":"keyModified"},{"id":"keyMissing"}]
				})"),
				absent,
				diagnostics);
			require(RowNamed(unbound.pages.front(), "keyPlain")
							.keybindInertState->governingReason ==
						InertReason::kKeybindUnbound,
				"an absent user file stopped meaning unbound");
		});
	}
}
