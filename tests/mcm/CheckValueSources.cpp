#include "../support/MCMTestSupport.h"
#include <array>
#include <string>
#include <tuple>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;
	using namespace support::mcm;

	void run_mcm_value_source_checks(Runner& runner)
	{
		runner.test("MCM source types resolve family and value kind", [] {
			// GlobalValue and PropertyValueEx carry no scalar suffix, so both
			// resolve to kNone; unrecognized strings keep raw but stay unknown.
			const auto result = ParseConfig(R"({
				"minMcmVersion":2,"modName":"Sources","displayName":"Sources",
				"content":[
					{"type":"section","text":"All"},
					{"id":"g","type":"slider","valueOptions":{"sourceType":"GlobalValue","min":0,"max":1,"step":1}},
					{"id":"pb","type":"switch","valueOptions":{"sourceType":"PropertyValueBool"}},
					{"id":"pi","type":"slider","valueOptions":{"sourceType":"PropertyValueInt","min":0,"max":1,"step":1}},
					{"id":"pf","type":"slider","valueOptions":{"sourceType":"PropertyValueFloat","min":0,"max":1,"step":1}},
					{"id":"ps","type":"input","valueOptions":{"sourceType":"PropertyValueString"}},
					{"id":"px","type":"slider","valueOptions":{"sourceType":"PropertyValueEx","min":0,"max":1,"step":1}},
					{"id":"mb:S","type":"switch","valueOptions":{"sourceType":"ModSettingBool"}},
					{"id":"mi:S","type":"slider","valueOptions":{"sourceType":"ModSettingInt","min":0,"max":1,"step":1}},
					{"id":"mf:S","type":"slider","valueOptions":{"sourceType":"ModSettingFloat","min":0,"max":1,"step":1}},
					{"id":"ms:S","type":"input","valueOptions":{"sourceType":"ModSettingString"}},
					{"id":"u","type":"input","valueOptions":{"sourceType":"Mystery"}}
				]
			})", "sources-config.json");
			require(result.configuration.has_value(),
				"source-type configuration did not parse");
			const auto& page = result.configuration->pages.front();
			const auto resolved = [&](std::string_view a_id) -> SourceType {
				const auto& control = ControlNamed(page, a_id);
				require(control.valueOptions &&
						control.valueOptions->sourceType.has_value(),
					"missing resolved source type: " + std::string{ a_id });
				return *control.valueOptions->sourceType;
			};

			const std::array<std::tuple<
				std::string_view,
				SourceFamily,
				SourceValueKind,
				std::string_view>, 11> expected{ {
				{ "g", SourceFamily::kGlobal, SourceValueKind::kNone, "GlobalValue" },
				{ "pb", SourceFamily::kProperty, SourceValueKind::kBool, "PropertyValueBool" },
				{ "pi", SourceFamily::kProperty, SourceValueKind::kInt, "PropertyValueInt" },
				{ "pf", SourceFamily::kProperty, SourceValueKind::kFloat, "PropertyValueFloat" },
				{ "ps", SourceFamily::kProperty, SourceValueKind::kString, "PropertyValueString" },
				{ "px", SourceFamily::kProperty, SourceValueKind::kNone, "PropertyValueEx" },
				{ "mb:S", SourceFamily::kModSetting, SourceValueKind::kBool, "ModSettingBool" },
				{ "mi:S", SourceFamily::kModSetting, SourceValueKind::kInt, "ModSettingInt" },
				{ "mf:S", SourceFamily::kModSetting, SourceValueKind::kFloat, "ModSettingFloat" },
				{ "ms:S", SourceFamily::kModSetting, SourceValueKind::kString, "ModSettingString" },
				{ "u", SourceFamily::kUnknown, SourceValueKind::kNone, "Mystery" }
			} };
			for (const auto& [id, family, value, raw] : expected)
			{
				const auto source = resolved(id);
				require(source.family == family && source.value == value &&
						source.raw == raw,
					"source type resolved incorrectly: " + std::string{ id });
			}
		});

	}
}
