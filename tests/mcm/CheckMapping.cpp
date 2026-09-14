#include "../support/MCMTestSupport.h"
#include <algorithm>
#include <optional>
#include <string>
#include <variant>

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

	}
}
