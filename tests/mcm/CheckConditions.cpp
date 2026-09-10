#include "../support/MCMTestSupport.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;
	using namespace support::mcm;

	void run_mcm_condition_checks(Runner& runner)
	{
		runner.test("MCM integer AND and OR group conditions retain structure", [] {
			const auto result = ParseConfig(R"json({
				"modName":"ConditionFixture",
				"content":[
					{"id":"integer","type":"text","groupCondition":2},
					{"id":"and","type":"text","groupCondition":{"AND":[1,{"OR":[2,3]}]}},
					{"id":"or","type":"text","groupCondition":{"OR":[3,4]}}
				]
			})json", "supported-conditions.json");
			require(result.configuration.has_value(),
				"condition fixture did not parse");
			const auto& page = result.configuration->pages.front();
			const auto& integer = ConditionNamed(page, "integer");
			const auto& all = ConditionNamed(page, "and");
			const auto& any = ConditionNamed(page, "or");
			require(integer.type == ConditionType::kControl &&
					integer.control == 2,
				"bare integer condition changed");
			require(all.type == ConditionType::kAll &&
					all.operands.size() == 2 &&
					all.operands[1].type == ConditionType::kAny,
				"nested AND and OR structure changed");
			require(any.type == ConditionType::kAny &&
					any.operands.size() == 2,
				"OR condition structure changed");
		});

		runner.test("MCM bare array group conditions stay outside FO4 scope", [] {
			const auto result = ParseConfig(R"json({
				"modName":"ArrayConditionFixture",
				"content":[
					{"id":"target","type":"text","groupCondition":[1,2]}
				]
			})json", "array-condition.json");
			require(result.configuration.has_value(),
				"array condition prevented the configuration from parsing");
			require(!ControlNamed(
						result.configuration->pages.front(),
						"target").groupCondition,
				"bare condition array stopped being dropped");
			require(HasDiagnostic(result,
						"expected a control number or condition object",
						"$.content[0].groupCondition"),
				"bare condition array stopped producing its current diagnostic");
		});

		runner.test("MCM unsupported condition shapes retain compatibility behavior", [] {
			const auto notCondition = ParseConfig(R"json({
				"modName":"NotConditionFixture",
				"content":[
					{"id":"target","type":"text","groupCondition":{"NOT":1}}
				]
			})json", "not-condition.json");
			const auto& notParsed = ConditionNamed(
				notCondition.configuration->pages.front(),
				"target");
			require(notParsed.type == ConditionType::kUnknown &&
					notParsed.rawOperator == "NOT" &&
					notParsed.operands.empty(),
				"NOT condition no longer has its current partial representation");
			require(HasDiagnostic(
						notCondition,
						"unknown condition operator 'NOT'") &&
					HasDiagnostic(
						notCondition,
						"condition operands must be an array"),
				"NOT condition diagnostics changed");

			const auto onlyCondition = ParseConfig(R"json({
				"modName":"OnlyConditionFixture",
				"content":[
					{"id":"target","type":"text","groupCondition":{"ONLY":[1,2]}}
				]
			})json", "only-condition.json");
			const auto& onlyParsed = ConditionNamed(
				onlyCondition.configuration->pages.front(),
				"target");
			require(onlyParsed.type == ConditionType::kUnknown &&
					onlyParsed.rawOperator == "ONLY" &&
					onlyParsed.operands.size() == 2,
				"ONLY condition no longer has its current partial representation");
			require(HasDiagnostic(
						onlyCondition,
						"unknown condition operator 'ONLY'"),
				"ONLY condition stopped producing its current diagnostic");

			const auto comparison = ParseConfig(R"json({
				"modName":"ComparisonConditionFixture",
				"content":[
					{"id":"target","type":"text","groupCondition":{
						"sourceSettingName":"bEnabled:Main",
						"operator":"==",
						"compareValue":true,
						"sourceType":"ModSettingBool"
					}}
				]
			})json", "comparison-condition.json");
			const auto& comparisonParsed = ConditionNamed(
				comparison.configuration->pages.front(),
				"target");
			require(comparisonParsed.type == ConditionType::kUnknown,
				"comparison object stopped producing a partial condition");
			require(HasDiagnostic(comparison,
						"condition object must have one operator") &&
					HasDiagnostic(comparison, "unknown condition operator") &&
					HasDiagnostic(comparison,
						"condition operands must be an array"),
				"comparison object diagnostics changed");
		});

		runner.test("MCM malformed multi-operator conditions are diagnosed", [] {
			const auto result = ParseConfig(R"json({
				"modName":"MultiConditionFixture",
				"content":[
					{"id":"target","type":"text",
					 "groupCondition":{"OR":[3,4],"AND":[5]}}
				]
			})json", "multi-condition.json");
			const auto& condition = ConditionNamed(
				result.configuration->pages.front(),
				"target");
			require(condition.type == ConditionType::kAll &&
					condition.operands.size() == 1 &&
					condition.operands.front().control == 5,
				"multi-operator object's current first-key behavior changed");
			require(HasDiagnostic(result,
						"condition object must have one operator"),
				"multi-operator object stopped producing its current diagnostic");
		});

		runner.test("MCM hidden condition state retains its binding", [] {
			const auto result = ParseConfig(kSyntheticConfig, "hidden-state.json");
			const auto& page = PageNamed(result, "$EXAMPLE_SOURCES");
			const auto hidden = std::ranges::find(
				page.rows,
				"InternalState",
				&MappedRow::id);
			require(hidden != page.rows.end() && !hidden->emitted &&
					hidden->binding &&
					std::holds_alternative<PropertyBinding>(
						hidden->binding->source),
				"hidden control did not retain its non-visual binding");
		});

		runner.test("MCM hidden controls retain their declared value type", [] {
			const auto result = ParseConfig(R"json({
				"modName":"HiddenValueType",
				"content":[{"id":"iState:Main","type":"hiddenSwitcher",
					"groupControl":1,"valueOptions":{
						"sourceType":"PropertyValueInt",
						"sourceForm":"Fixture.esp|1",
						"propertyName":"State"}}]
			})json", "hidden-value-type.json");
			const auto& binding = *result.pages.front().rows.front().binding;
			require(binding.valueKind == SourceValueKind::kInt &&
					std::holds_alternative<int64_t>(binding.target),
				"an integer hidden property was forced into a boolean target");
		});

		runner.test("MCM idless local ownership keeps conservative guardrails", [] {
			const auto checkPersistentFailure =
				[](std::string_view a_name,
					std::string_view a_control,
					std::string_view a_dependent,
					std::string_view a_message) {
					const auto result = ParseConfig(
						std::string{
							R"({"modName":"Guardrail","displayName":"Guardrail","content":[)"
						} +
							std::string{ a_control } +
							(a_dependent.empty() ?
								 std::string{} :
								 "," + std::string{ a_dependent }) +
							"]}",
						a_name);
					require(result.pages.size() == 1 &&
							ErrorCount(result) == 1 &&
							DiagnosticCount(result, a_message) == 1,
						"guardrail did not retain one binding diagnostic: " +
							std::string{ a_name } + ErrorMessages(result));
					const auto& row = result.pages.front().rows.front();
					require(row.valueRoute == ValueRoute::kSource &&
							!row.binding &&
							row.unmappedSource,
						"invalid persistent control became local: " +
							std::string{ a_name });
				};

			checkPersistentFailure(
				"unreferenced.json",
				R"({"type":"switcher","groupControl":1,"valueOptions":{"sourceType":"ModSettingBool"}})",
				{},
				"valid setting id");
			checkPersistentFailure(
				"not-controller.json",
				R"({"type":"switcher","valueOptions":{"sourceType":"ModSettingBool"}})",
				R"({"id":"dependent","type":"text","groupCondition":1})",
				"valid setting id");
			checkPersistentFailure(
				"wrong-family.json",
				R"({"type":"switcher","groupControl":1,"valueOptions":{"sourceType":"GlobalValue"}})",
				R"({"id":"dependent","type":"text","groupCondition":1})",
				"form identifier");
			checkPersistentFailure(
				"wrong-value-kind.json",
				R"({"type":"switcher","groupControl":1,"valueOptions":{"sourceType":"ModSettingInt"}})",
				R"({"id":"dependent","type":"text","groupCondition":1})",
				"valid setting id");
			checkPersistentFailure(
				"nonboolean-control.json",
				R"({"type":"slider","groupControl":1,"valueOptions":{"sourceType":"ModSettingBool","min":0,"max":1,"step":1}})",
				R"({"id":"dependent","type":"text","groupCondition":1})",
				"valid setting id");
			checkPersistentFailure(
				"invalid-id.json",
				R"({"id":"bInvalid:","type":"switcher","groupControl":1,"valueOptions":{"sourceType":"ModSettingBool"}})",
				R"({"id":"dependent","type":"text","groupCondition":1})",
				"valid setting id");

			const auto unsupported = ParseConfig(R"json({
				"modName":"UnsupportedGuardrail",
				"displayName":"Unsupported guardrail",
				"content":[
					{"type":"color","groupControl":1,
					 "valueOptions":{"sourceType":"ModSettingBool"}},
					{"id":"dependent","type":"text","groupCondition":1}
				]
			})json", "unsupported-guardrail.json");
			require(ErrorCount(unsupported) == 1 &&
					HasDiagnostic(unsupported, "unsupported in this phase") &&
					unsupported.pages.front().rows.front().valueRoute ==
						ValueRoute::kSource,
				"unsupported control was promoted or lost its diagnostic");
		});

		runner.test("MCM valid JSON escapes retain strict semantics", [] {
			const auto result = ParseConfig(R"json({
				"modName":"ValidEscapeFixture",
				"content":[
					{"id":"escaped","type":"text",
					 "help":"line\nquote \" slash \\ unicode \u0041"}
				]
			})json", "valid-escapes.json");
			require(result.configuration.has_value(),
				"valid JSON escapes prevented parsing");
			require(ControlNamed(
						result.configuration->pages.front(),
						"escaped").help ==
					"line\nquote \" slash \\ unicode A",
				"valid JSON escapes changed meaning");
		});

		runner.test("MCM invalid path escapes pass through as literals", [] {
			for (const auto escape : { 'U', 'D', 'M', 'F', 'P', 'O', 'Y', 'L',
					 'S' })
			{
				auto json = std::string{
					R"json({"modName":"InvalidEscapeFixture","content":[{"id":"help","type":"text","help":"C:\)json" };
				json.push_back(escape);
				json += R"json(ser\Folder"}]})json";
				const auto result = ParseConfig(json, "invalid-escape.json");
				require(result.configuration && result.pages.size() == 1,
					"invalid path escape was not normalized");
				auto expected = std::string{ "C:\\" };
				expected.push_back(escape);
				expected += "ser\\Folder";
				require(ControlNamed(
							result.configuration->pages.front(),
							"help").help == expected,
					"invalid escape did not pass through literally");
			}
		});

	}
}
