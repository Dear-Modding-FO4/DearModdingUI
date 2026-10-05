#include "../support/MCMTestSupport.h"
#include <string>
#include <string_view>
#include <variant>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;
	using namespace support::mcm;

	void run_mcm_condition_checks(Runner& runner)
	{
		runner.test("MCM hidden controls retain nonvisual typed bindings", [] {
			const auto result = ParseConfig(R"json({
				"modName":"HiddenValueType",
				"content":[{"id":"iState:Main","type":"hiddenSwitcher",
					"groupControl":1,"valueOptions":{
						"sourceType":"PropertyValueInt",
						"sourceForm":"Fixture.esp|1",
						"propertyName":"State"}}]
			})json", "hidden-value-type.json");
			const auto& row = result.pages.front().rows.front();
			require(!row.emitted && row.binding &&
					std::holds_alternative<PropertyBinding>(
						row.binding->source) &&
					row.binding->valueKind == SourceValueKind::kInt &&
					std::holds_alternative<int64_t>(row.binding->target),
				"hidden control lost its nonvisual typed property binding");
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

		runner.test("MCM invalid path escapes pass through as literals", [] {
			const auto valid = ParseConfig(R"json({
				"modName":"ValidEscapeFixture",
				"content":[
					{"id":"escaped","type":"text",
					 "help":"line\nquote \" slash \\ unicode \u0041"}
				]
			})json", "valid-escapes.json");
			require(valid.configuration &&
					ControlNamed(
						valid.configuration->pages.front(),
						"escaped").help ==
						"line\nquote \" slash \\ unicode A",
				"valid JSON escapes changed meaning");

			for (const auto escape : { 'U', 'P' })
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
