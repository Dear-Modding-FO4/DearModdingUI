#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/SettingsIni.h>

#include "Harness.h"

#include <array>
#include <filesystem>
#include <string_view>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI::MCM;

		constexpr std::string_view kSettingsIniFixture = R"ini([Main]
bEnabled=1
iRetries=3
fScale=1.25
sProfile=Default:Careful

[Advanced]
bDiagnostics=0
)ini";

		struct SettingIdCase
		{
			std::string_view id;
			std::string_view name;
			std::string_view section;
			bool valid;
		};

		constexpr std::array kSettingIdCases{
			SettingIdCase{ "bEnabled", "bEnabled", "Main", true },
			SettingIdCase{ "iRetries:Advanced", "iRetries", "Advanced", true },
			SettingIdCase{ "fScale:", "fScale", "", false },
			SettingIdCase{ ":Advanced", "", "Advanced", false },
			SettingIdCase{ "sProfile:Main:Extra", "sProfile", "Main:Extra", false }
		};

		constexpr std::string_view kUndeclaredControlsConfig = R"json({
			"modName":"FixtureBridge",
			"displayName":"Fixture Bridge",
			"content":[
				{"id":"bEntry01:Troubleshooting","type":"switcher","valueOptions":{"sourceType":"ModSettingBool"}},
				{"id":"bEntry02:Troubleshooting","type":"switcher","valueOptions":{"sourceType":"ModSettingBool"}},
				{"id":"bEntry03:Troubleshooting","type":"switcher","valueOptions":{"sourceType":"ModSettingBool"}},
				{"id":"bEntry04:Troubleshooting","type":"switcher","valueOptions":{"sourceType":"ModSettingBool"}},
				{"id":"bEntry05:Troubleshooting","type":"switcher","valueOptions":{"sourceType":"ModSettingBool"}},
				{"id":"bEntry06:Troubleshooting","type":"switcher","valueOptions":{"sourceType":"ModSettingBool"}}
			]
		})json";
	}

	void run_mcm_settings_ini_checks(Runner& runner)
	{
		runner.test("MCM settings ini declarations are parsed", [] {
			const auto settings = ParseSettingsIni(kSettingsIniFixture);
			require(settings.available && settings.declarations.size() == 5,
				"settings declarations were not collected");
			require(settings.Contains({ "bEnabled", "Main" }) &&
					settings.Contains({ "sProfile", "Main" }) &&
					settings.Contains({ "bDiagnostics", "Advanced" }),
				"section or typed key declarations were lost");
		});

		runner.test("MCM setting ids normalize section and key", [] {
			const auto implicit = ParseSettingIdentifier(kSettingIdCases[0].id);
			const auto explicitSection =
				ParseSettingIdentifier(kSettingIdCases[1].id);
			require(implicit && implicit->section == "Main" &&
					implicit->key == "bEnabled",
				"a missing section no longer defaults to Main");
			require(explicitSection &&
					explicitSection->key == "iRetries" &&
					explicitSection->section == "Advanced",
				"an explicit section contract changed");
			require(!ParseSettingIdentifier(kSettingIdCases[2].id),
				"an empty section stopped being malformed");
			require(!ParseSettingIdentifier(kSettingIdCases[3].id) &&
					!ParseSettingIdentifier(kSettingIdCases[4].id),
				"malformed setting ids stopped being represented");
		});

		runner.test("MCM undeclared bool controls are detectable", [] {
			auto result = ParseConfig(
				kUndeclaredControlsConfig,
				"undeclared-controls.json");
			require(result.configuration && result.pages.size() == 1,
				"undeclared-control fixture did not map six bindings");
			ApplyDeclarations(
				result.pages.front(),
				ParseSettingsIni(kSettingsIniFixture));
			auto count = size_t{};
			for (const auto& row : result.pages.front().rows)
			{
				if (!row.binding)
					continue;
				const auto* binding =
					std::get_if<ModSettingBinding>(&row.binding->source);
				require(binding &&
						binding->declaration == DeclarationState::kUndeclared,
					"an absent declaration was not marked undeclared");
				++count;
			}
			require(count == 6,
				"undeclared-control fixture did not map six bindings");
			require(SummarizeCompatibility(result.pages.front())
						.undeclaredModSettings == 6,
				"compatibility summary lost undeclared settings");
		});

		runner.test("MCM settings ini ignores comments and malformed lines", [] {
			const auto settings = ParseSettingsIni(R"ini(
; comment
# comment
malformed
=missing
bImplicit=1
[Main]
bEnabled=1
[ ]
bIgnored=1
[Advanced]
sPath=C:\Games:Fallout
)ini");
			require(settings.declarations.size() == 3 &&
					settings.Contains({ "bImplicit", "Main" }) &&
					settings.Contains({ "bEnabled", "Main" }) &&
					settings.Contains({ "sPath", "Advanced" }),
				"comments, malformed lines, or colons in values changed declarations");
		});

		runner.test("MCM absent settings ini leaves declarations unknown", [] {
			const auto settings = LoadSettingsIni(
				std::filesystem::temp_directory_path() /
				"dmui-mcm-absent-settings.ini");
			require(!settings.available && settings.declarations.empty(),
				"absent settings.ini did not produce unknown declaration state");
		});
	}
}
