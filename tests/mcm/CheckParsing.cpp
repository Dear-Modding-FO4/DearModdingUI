#include "../support/MCMTestSupport.h"
#include <fstream>
#include <optional>
#include <string>
#include <variant>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;
	using namespace support::mcm;

	void run_mcm_parsing_checks(Runner& runner)
	{
		runner.test("MCM LoadConfig reads a minimal temporary file", [] {
			const auto path = TemporaryConfigPath("load-success");
			const TemporaryFileCleanup cleanup{ path };
			{
				std::ofstream file(path, std::ios::binary);
				require(file.is_open(),
					"temporary MCM configuration could not be created");
				file << R"json({
					"modName":"FileFixture",
					"displayName":"$FILE_TITLE",
					"content":[
						{"id":"summary","type":"text","text":"Loaded from disk"}
					]
				})json";
				require(file.good(),
					"temporary MCM configuration could not be written");
			}

			const auto result = LoadConfig(path);
			require(result.configuration &&
					result.configuration->modName == "FileFixture" &&
					result.pages.size() == 1 &&
					DescriptorCount(result.pages.front()) == 1 &&
					ErrorCount(result) == 0,
				"temporary MCM configuration did not load");
			const auto localized = LoadConfig(
				path,
				[](std::string_view a_key) -> std::optional<std::string> {
					return a_key == "$FILE_TITLE" ?
						std::optional<std::string>{ "Localized file title" } :
						std::optional<std::string>{ std::string{ a_key } };
				});
			require(localized.pages.front().displayName ==
						"Localized file title" &&
					localized.configuration->displayName == "$FILE_TITLE",
				"LoadConfig did not thread display localization");

			const auto missingPath = TemporaryConfigPath("does-not-exist");
			const auto missing = LoadConfig(missingPath);
			require(!missing.configuration && missing.pages.empty() &&
					HasDiagnostic(missing, "could not open") &&
					missing.diagnostics.front().source.find(
						"does-not-exist") != std::string::npos,
				"absent file did not return an empty path-located diagnostic");
		});

		runner.test("MCM malformed JSON returns a located diagnostic", [] {
			const auto result = ParseConfig(
				R"({"modName":"Broken","content":[)",
				"truncated-config.json");
			require(!result.configuration && result.pages.empty(),
				"truncated JSON produced configuration data");
			require(HasDiagnostic(result, "invalid JSON", "$"),
				"truncated JSON was not diagnosed");
			require(result.diagnostics.front().source == "truncated-config.json",
				"malformed JSON diagnostic lost its source");
		});

		runner.test("MCM missing setting metadata remains total and visible", [] {
			const auto result = ParseConfig(R"({
				"minMcmVersion": 2,
				"modName": "Incomplete",
				"displayName": "Incomplete",
				"content": [
					{"id":"switch","text":"Switch","type":"switch"},
					{"id":"slider","text":"Slider","type":"slider",
					 "valueOptions":{"sourceType":"ModSettingFloat"}},
					{"id":"typeless","text":"Typeless"},
					{"id":"unknown","text":"Unknown","type":"dial"}
				]
			})", "incomplete-config.json");
			require(result.configuration && result.pages.size() == 1 &&
					DescriptorCount(result.pages.front()) == 4,
				"incomplete controls disappeared");
			require(std::holds_alternative<dmui::CheckboxSettingControl>(
						SettingNamed(result.pages.front(), "switch").control),
				"switch without valueOptions did not preserve its shape");
			require(std::holds_alternative<dmui::DoubleSettingControl>(
						SettingNamed(result.pages.front(), "slider").control),
				"range-less slider did not preserve its shape");
			const auto& slider =
				std::get<dmui::DoubleSettingControl>(
					SettingNamed(result.pages.front(), "slider").control);
			require(slider.range &&
					slider.range->minimum == std::optional<double>{ 0.0 } &&
					slider.range->maximum == std::optional<double>{ 1.0 } &&
					!slider.quantization &&
					RowNamed(
						result.pages.front(),
						"slider").sliderNormalization,
				"range-less slider did not inherit MCM widget defaults");
			require(std::holds_alternative<dmui::UnsupportedSettingControl>(
						SettingNamed(result.pages.front(), "typeless").control),
				"typeless control did not degrade to unsupported");
			require(std::holds_alternative<dmui::UnsupportedSettingControl>(
						SettingNamed(result.pages.front(), "unknown").control) &&
					HasDiagnostic(
						result,
						"unknown MCM control type 'dial' maps to unsupported",
						"$.content[3]"),
				"unknown control did not degrade with its diagnostic");
			require(HasDiagnostic(result, "missing valueOptions",
						"$.content[0]") &&
					HasDiagnostic(result, "missing required string",
						"$.content[2].type"),
				"incomplete controls were not fully diagnosed");
		});

		runner.test("MCM empty sections divide an existing named group", [] {
			const auto result = ParseConfig(R"({
				"modName": "DividedSection",
				"displayName": "Divided Section",
				"content": [
					{"id":"heading","type":"section","text":"Questions"},
					{"id":"first","type":"switcher"},
					{"id":"divider","type":"section","text":""},
					{"id":"second","type":"switcher"}
				]
			})", "divided-section-config.json");
			require(result.pages.size() == 1 &&
					result.pages.front().settings.groups.size() == 1,
				"empty section split an existing named group");
			const auto& group = result.pages.front().settings.groups.front();
			require(group.id == "heading" && group.label == "Questions" &&
					group.headingMode ==
						dmui::SettingGroup::HeadingMode::kAutomatic &&
					group.settings.size() == 2 &&
					group.rows.size() == 3 &&
					std::holds_alternative<dmui::SettingGroup::DividerRow>(
						group.rows[1]),
				"empty section did not preserve a divider row in source order");

			const auto standalone = ParseConfig(R"({
				"modName":"EmptySection",
				"content":[
					{"id":"divider","type":"section","text":""},
					{"id":"enabled","type":"switcher"}
				]
			})", "empty-section-config.json");
			const auto& standaloneGroup =
				standalone.pages.front().settings.groups.front();
			require(standaloneGroup.id == "divider" &&
					standaloneGroup.label.empty() &&
					standaloneGroup.glyph == U'\0' &&
					standaloneGroup.headingMode ==
						dmui::SettingGroup::HeadingMode::kDivider,
				"a leading empty section did not produce an unnamed group");
		});

		runner.test("MCM deeply nested conditions are diagnosed not fatal", [] {
			std::string json =
				R"({"minMcmVersion":2,"modName":"Deep","displayName":"Deep",)"
				R"("content":[{"id":"target","type":"switch",)"
				R"("valueOptions":{"sourceType":"ModSettingBool"},)"
				R"("groupCondition":)";
			constexpr size_t depth = 5000;
			for (size_t index = 0; index < depth; ++index)
				json += R"({"AND":[)";
			json += "1";
			for (size_t index = 0; index < depth; ++index)
				json += "]}";
			json += "}]}";

			const auto result = ParseConfig(json, "deep-config.json");
			require(result.configuration.has_value(),
				"deeply nested condition prevented parsing");
			require(HasDiagnostic(result, "condition nesting exceeds"),
				"excessive condition nesting was not diagnosed");
			const auto& control =
				result.configuration->pages.front().controls.front();
			require(control.groupCondition.has_value(),
				"deep condition produced no partial result");
		});

	}
}
