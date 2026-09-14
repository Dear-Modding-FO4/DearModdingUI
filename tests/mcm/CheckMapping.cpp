#include "../support/MCMTestSupport.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;
	using namespace support::mcm;

	void run_mcm_mapping_structure_checks(Runner& runner)
	{
		runner.test("MCM mapping preserves root and group boundaries", [] {
			const auto result = ParseConfig(R"json({
				"modName":"Structure",
				"displayName":"Structure",
				"content":[
					{"id":"shared","type":"section","text":"Primary"},
					{"id":"first","type":"text","text":"First"},
					{"id":"shared","type":"section","text":"Secondary"},
					{"id":"second","type":"text","text":"Second"}
				],
				"pages":[{
					"id":"advanced",
					"pageDisplayName":"Advanced",
					"content":[
						{"id":"shared","type":"section","text":"Advanced"},
						{"id":"third","type":"text","text":"Third"}
					]
				}]
			})json", "mapping-structure.json");
			require(result.configuration.has_value(),
				"mapping structure fixture did not parse");
			require(ErrorCount(result) == 0,
				"mapping structure fixture produced parse errors: " +
					ErrorMessages(result));
			require(result.pages.size() == 2 &&
					result.configuration->pages[0].root &&
					result.pages[0].id == "main" &&
					!result.configuration->pages[1].root &&
					result.pages[1].id == "advanced",
				"root and named page ordering changed");
			const auto& rootGroups = result.pages[0].settings.groups;
			require(
				rootGroups.size() == 2 &&
					rootGroups[0].id == "shared" &&
					rootGroups[0].label == "Primary" &&
					rootGroups[0].settings.size() == 1 &&
					rootGroups[0].settings.front().id == "first" &&
					rootGroups[1].id == "shared-2" &&
					rootGroups[1].label == "Secondary" &&
					rootGroups[1].settings.size() == 1 &&
					rootGroups[1].settings.front().id == "second",
				"duplicate group ids or section boundaries changed");
			require(
				result.pages[1].settings.groups.size() == 1 &&
					result.pages[1].settings.groups.front().id == "shared" &&
					result.pages[1].settings.groups.front().settings.size() == 1 &&
					result.pages[1].settings.groups.front().settings.front().id ==
						"third",
				"group identity leaked across page boundaries");
		});

		runner.test("MCM synthetic config maps controls ranges and labels", [] {
			const auto result = ParseConfig(
				kSyntheticConfig,
				"synthetic-config.json");
			const auto& page = PageNamed(result, "$EXAMPLE_CONTROLS");

			const auto& enabled = SettingNamed(page, "EnableFeature");
			require(std::holds_alternative<dmui::CheckboxSettingControl>(
						enabled.control),
				"switcher did not map to checkbox");
			require(enabled.label == "$EXAMPLE_ENABLE" &&
					enabled.description == "$EXAMPLE_ENABLE_HELP",
				"switcher text or help changed");

			const auto& mode = SettingNamed(page, "DisplayMode");
			const auto* choice =
				std::get_if<dmui::ChoiceSettingControl>(&mode.control);
			require(choice && choice->options.size() == 3,
				"dropdown did not map all ordered choices");
			require(choice->options.front().value == "0" &&
					choice->options.front().label == "$EXAMPLE_MODE_CALM",
				"dropdown option order changed");

			const auto& sensitivity =
				SettingNamed(page, "fSensitivity:SampleTweaks");
			const auto* numeric =
				std::get_if<dmui::DoubleSettingControl>(&sensitivity.control);
			require(numeric && numeric->range &&
					numeric->range->minimum &&
					numeric->range->maximum &&
					!numeric->quantization,
				"float slider range was not mapped");
			RequireNear(*numeric->range->minimum, 0.25);
			RequireNear(*numeric->range->maximum, 2.5);
			const auto* sensitivityNormalization =
				std::get_if<DoubleSliderNormalization>(
					&*RowNamed(
						page,
						"fSensitivity:SampleTweaks").sliderNormalization);
			require(sensitivityNormalization,
				"float slider normalization metadata was not retained");
			RequireNear(sensitivityNormalization->step, 0.05);
			RequireNear(numeric->dragSpeed, 0.0);
			require(sensitivity.label == "$EXAMPLE_SENSITIVITY" &&
					sensitivity.description == "$EXAMPLE_SENSITIVITY_HELP",
				"slider text or help changed");

			const auto& retries =
				SettingNamed(page, "iRetryCount:SampleTweaks");
			const auto* integer =
				std::get_if<dmui::SignedSettingControl>(&retries.control);
			require(integer && integer->range &&
					integer->range->minimum == std::optional<int64_t>{ 1 } &&
					integer->range->maximum == std::optional<int64_t>{ 8 } &&
					!integer->quantization,
				"integer slider did not retain its range and quantization");
			const auto* retryNormalization =
				std::get_if<SignedSliderNormalization>(
					&*RowNamed(
						page,
						"iRetryCount:SampleTweaks").sliderNormalization);
			require(retryNormalization &&
					retryNormalization->step == 1,
				"integer slider normalization metadata was not retained");
			require(ControlKindCount(
						result,
						dmui::SettingControlKind::kDouble) == 2 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kSigned) == 1,
				"synthetic numeric control kinds changed");
		});

		runner.test("MCM sliders without max use upstream widget defaults", [] {
			const auto result = ParseConfig(R"json({
				"modName":"Steps",
				"content":[
					{"id":"float","type":"slider","valueOptions":{
						"sourceType":"GlobalValueFloat",
						"sourceForm":"Fixture.esp|1","step":0.25}},
					{"id":"integer","type":"slider","valueOptions":{
						"sourceType":"GlobalValueInt",
						"sourceForm":"Fixture.esp|2","step":2}}
				]
			})json");
			const auto* floating = std::get_if<dmui::DoubleSettingControl>(
				&SettingNamed(result.pages.front(), "float").control);
			const auto* integer = std::get_if<dmui::SignedSettingControl>(
				&SettingNamed(result.pages.front(), "integer").control);
			const auto* floatNormalization =
				std::get_if<DoubleSliderNormalization>(
					&*RowNamed(
						result.pages.front(),
						"float").sliderNormalization);
			const auto* integerNormalization =
				std::get_if<SignedSliderNormalization>(
					&*RowNamed(
						result.pages.front(),
						"integer").sliderNormalization);
			require(floating && !floating->quantization &&
					floating->range &&
					floating->range->minimum == std::optional<double>{ 0.0 } &&
					floating->range->maximum == std::optional<double>{ 1.0 } &&
					floatNormalization &&
					floatNormalization->step == 0.05 &&
					integer && !integer->quantization &&
					integer->range &&
					integer->range->minimum == std::optional<int64_t>{ 0 } &&
					integer->range->maximum == std::optional<int64_t>{ 1 } &&
					integerNormalization &&
					integerNormalization->step == 1,
				"an omitted max did not preserve MCM's 0..1 widget defaults");
		});

		runner.test("MCM slider parameter edge cases stay explicit and safe", [] {
			const auto result = ParseConfig(R"json({
				"modName":"SliderEdges",
				"content":[
					{"id":"fNullMax:S","type":"slider","valueOptions":{
						"sourceType":"ModSettingFloat",
						"min":7,"max":null,"step":2}},
					{"id":"fNullMin:S","type":"slider","valueOptions":{
						"sourceType":"ModSettingFloat",
						"min":null,"max":10,"step":1}},
					{"id":"fMaxOnly:S","type":"slider","valueOptions":{
						"sourceType":"ModSettingFloat",
						"max":10}},
					{"id":"fBadMax:S","type":"slider","valueOptions":{
						"sourceType":"ModSettingFloat",
						"min":0,"max":"ten","step":1}},
					{"id":"fBackwards:S","type":"slider","valueOptions":{
						"sourceType":"ModSettingFloat",
						"min":10,"max":1,"step":1}}
				]
			})json", "slider-edges.json");
			const auto* nullMax = std::get_if<dmui::DoubleSettingControl>(
				&SettingNamed(result.pages.front(), "fNullMax:S").control);
			const auto* nullMin = std::get_if<dmui::DoubleSettingControl>(
				&SettingNamed(result.pages.front(), "fNullMin:S").control);
			require(nullMax && nullMax->range &&
					nullMax->range->minimum == std::optional<double>{ 0.0 } &&
					nullMax->range->maximum == std::optional<double>{ 1.0 } &&
					!nullMax->quantization &&
					RowNamed(
						result.pages.front(),
						"fNullMax:S").sliderNormalization &&
					nullMin && nullMin->range &&
					nullMin->range->minimum == std::optional<double>{ 0.0 } &&
					nullMin->range->maximum == std::optional<double>{ 10.0 } &&
					!nullMin->quantization &&
					RowNamed(
						result.pages.front(),
						"fNullMin:S").sliderNormalization,
				"null max/min did not follow verified AVM2 slider semantics");
			for (const auto id : {
					"fMaxOnly:S",
					"fBadMax:S",
					"fBackwards:S" })
			{
				const auto row = std::ranges::find(
					result.pages.front().rows,
					id,
					&MappedRow::id);
				require(row != result.pages.front().rows.end() &&
						row->unsupported,
					"malformed slider stayed operable: " +
						std::string{ id });
			}
			require(HasDiagnostic(
						result,
						"requires finite numeric min, max, and positive step",
						"$.content[2]") &&
					HasDiagnostic(result, "expected a number", "$.content[3]") &&
					HasDiagnostic(result, "maximum is less", "$.content[4]"),
				"malformed slider diagnostics were incomplete");
		});

		runner.test("MCM mapper gates markup on the HTML declaration", [] {
			const auto result = ParseConfig(R"({
				"modName":"Markup",
				"content":[
					{"id":"rich","type":"text","html":true,"align":"right",
					 "text":"<p align='center'><i>Rich</i><br />text</p>"},
					{"id":"literal","type":"text","html":false,
					 "text":"Literal <Press E>"},
					{"id":"numeric","type":"text","html":1,
					 "text":"<i>Numeric</i>"},
					{"id":"string","type":"text","html":"false",
					 "text":"<i>String</i>"},
					{"id":"absent","type":"text","text":"<i>Absent</i>"}
				]
			})", "markup-config.json");
			require(result.pages.size() == 1,
				"markup configuration did not map");
			const auto& page = result.pages.front();
			const auto richText = std::ranges::find(
				page.rows,
				"rich",
				&MappedRow::id);
			require(
				std::get<std::string>(
					SettingNamed(page, "rich").defaultValue) ==
						"Rich\ntext" &&
					std::get<std::string>(
						SettingNamed(page, "literal").defaultValue) ==
						"Literal <Press E>" &&
					std::get<std::string>(
						SettingNamed(page, "numeric").defaultValue) ==
						"Numeric" &&
					std::get<std::string>(
						SettingNamed(page, "string").defaultValue) ==
						"String" &&
					std::get<std::string>(
						SettingNamed(page, "absent").defaultValue) ==
						"<i>Absent</i>",
				"mapper did not honor representative HTML truthiness values");
			require(richText != page.rows.end() && richText->text &&
					richText->text->presentation.text == "Rich\ntext" &&
					richText->text->presentation.alignment ==
						TextAlignment::kCenter,
				"mapper lost resolved text presentation data");
			require(
				!HasDiagnostic(result, "not represented"),
				"rendered text presentation retained an unsupported warning");
		});

	}
}
