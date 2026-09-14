#include "../support/MCMTestSupport.h"

#include <DearModdingUI/MCM/Keybinds.h>
#include <DearModdingUI/MCM/SettingsIni.h>

#include <filesystem>
#include <string>
#include <variant>

namespace vmm_tests
{
	void run_mcm_integration_fixture_checks(Runner& runner)
	{
		using namespace DearModdingUI::MCM;
		runner.test("packaged MCM fixture maps with declared storage and keybinds", [] {
			const std::filesystem::path root{
				"tools/shared/fixtures/mcm/data/MCM/Config/DMUITests"
			};
			auto fixture = LoadConfig(root / "config.json");
			require(fixture.configuration && !fixture.pages.empty(),
				"packaged MCM fixture did not load");
			std::string issues;
			for (const auto& diagnostic : fixture.diagnostics)
				issues += diagnostic.location + ": " + diagnostic.message + "\n";
			require(issues.empty(), "packaged MCM fixture has diagnostics: " + issues);

			const auto declarations = LoadSettingsIni(root / "settings.ini");
			const auto keybinds = LoadKeybindDefinitions(root / "keybinds.json");
			require(declarations.available &&
					keybinds.state == KeybindFileState::kLoaded &&
					keybinds.modName == fixture.configuration->modName,
				"packaged MCM fixture lost its storage or keybind declarations");
			for (auto& page : fixture.pages)
			{
				ApplyDeclarations(page, declarations);
				for (const auto& row : page.rows)
				{
					require(!row.unsupported,
						"packaged MCM control is unsupported: " + row.id);
					if (row.keybindId)
						require(keybinds.Contains(*row.keybindId),
							"packaged MCM keybind is undeclared: " + *row.keybindId);
					if (!row.binding)
						continue;
					const auto* setting =
						std::get_if<ModSettingBinding>(&row.binding->source);
					if (setting && !setting->key.empty())
						require(setting->declaration == DeclarationState::kDeclared,
							"packaged MCM setting cannot persist: " + row.id);
				}
			}
		});
	}
}
