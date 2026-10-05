#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/SettingsIni.h>

#include "../Harness.h"

#include <filesystem>
#include <string_view>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI::MCM;

		constexpr std::string_view kSettingsIniFixture = R"ini(
; leading comment
# another comment
malformed
=missing
bImplicit=1
[Main]
bEnabled=1
iRetries=3
fScale=1.25
sProfile=Default:Careful
bEnabled=0

[Advanced]
bDiagnostics=0
)ini";
	}

	void run_mcm_settings_ini_checks(Runner& runner)
	{
		runner.test("MCM settings ini declarations are parsed", [] {
			const auto settings = ParseSettingsIni(kSettingsIniFixture);
			require(settings.available && settings.declarations.size() == 6,
				"settings declarations were not collected");
			require(settings.Contains({ "bImplicit", "Main" }) &&
					settings.Contains({ "bEnabled", "Main" }) &&
					settings.Contains({ "sProfile", "Main" }) &&
					settings.Contains({ "bDiagnostics", "Advanced" }),
				"sections, comments, malformed lines, duplicates, or colons changed declarations");

			auto mapped = ParseConfig(R"json({
				"modName":"DeclarationFixture",
				"content":[
					{"id":"bEnabled:Main","type":"switcher",
					 "valueOptions":{"sourceType":"ModSettingBool"}},
					{"id":"bMissing:Main","type":"switcher",
					 "valueOptions":{"sourceType":"ModSettingBool"}}
				]
			})json", "declarations.json");
			require(mapped.pages.size() == 1,
				"declaration fixture did not map");
			auto& page = mapped.pages.front();
			require(page.rows.size() == 2 &&
					page.rows[0].binding &&
					page.rows[1].binding,
				"declaration fixture did not retain both bindings");
			ApplyDeclarations(page, settings);
			require(
				std::get<ModSettingBinding>(
					page.rows[0].binding->source).declaration ==
						DeclarationState::kDeclared &&
					std::get<ModSettingBinding>(
						page.rows[1].binding->source).declaration ==
						DeclarationState::kUndeclared,
				"matching and missing declarations were not applied");

			const auto implicit = ParseSettingIdentifier("bEnabled");
			require(implicit && implicit->section == "Main" &&
					implicit->key == "bEnabled",
				"a missing section no longer defaults to Main");
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
