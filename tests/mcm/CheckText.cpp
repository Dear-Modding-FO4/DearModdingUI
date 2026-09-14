#include "../support/MCMTestSupport.h"
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace vmm_tests
{
	using namespace DearModdingUI::MCM;
	using namespace support::mcm;

	void run_mcm_text_checks(Runner& runner)
	{
		runner.test("MCM text presentation handles rich literal and malformed markup", [] {
			const auto rich = ResolveTextPresentation(
				"A<br>B<p>Plain</p><p ALIGN='right'>&lt;Right&gt; &#65;</p>",
				true,
				"center");
			require(rich.text == "A\nB\nPlain\n<Right> A" &&
					rich.alignment == TextAlignment::kRight,
				"breaks, paragraphs, entities, or paragraph alignment changed");

			const auto literal = ResolveTextPresentation(
				"<Press E> <i>literal</i> &amp;",
				false,
				"center");
			require(literal.text == "<Press E> <i>literal</i> &amp;" &&
					literal.alignment == TextAlignment::kCenter,
				"literal markup or control alignment changed");

			const auto malformed = ResolveTextPresentation(
				"Keep < stray <i>open</i> <font size='30' broken &bogus; tail",
				true);
			require(
				malformed.text ==
					"Keep < stray open <font size='30' broken &bogus; tail",
				"malformed HTML lost readable source text");
		});

		runner.test("MCM display localization preserves identities and stored values", [] {
			const TextResolver resolver = [](std::string_view a_key)
				-> std::optional<std::string> {
				if (a_key == "$CLIENT NAME")
					return "Localized Client";
				if (a_key == "$LABEL")
					return "Localized Label";
				if (a_key == "$OPTION")
					return "Localized Option";
				return std::nullopt;
			};
			constexpr auto json = R"json({
				"modName":"IdentityMod",
				"displayName":"$CLIENT NAME",
				"content":[
					{"id":"sMode:Main","type":"dropdown","text":"$LABEL",
					 "valueOptions":{"sourceType":"ModSettingString",
					 "default":"$OPTION","options":["$OPTION","literal"]}},
					{"id":"action","type":"button","text":"$LABEL",
					 "action":{"type":"SendEvent","event":"$EVENT","params":["$ARG"]}},
					{"id":"rich","type":"text",
					 "text":"<b>Prefix</b> $OPTION","html":true},
					{"id":"literal","type":"text",
					 "text":"<b>Prefix</b> $OPTION"}
				]
			})json";
			const auto result = ParseConfig(json, "localized.json", resolver);
			require(result.configuration && result.pages.size() == 1 &&
					result.diagnostics.empty(),
				"localized fixture did not map cleanly");
			require(result.configuration->modName == "IdentityMod" &&
					result.configuration->displayName == "$CLIENT NAME" &&
					result.displayName == "Localized Client" &&
					result.pages.front().id == "main",
				"localized presentation changed raw configuration identity");

			const auto& root = result.pages.front();
			const auto& choice = SettingNamed(root, "sMode:Main");
			const auto* choiceControl =
				std::get_if<dmui::ChoiceSettingControl>(&choice.control);
			require(choice.label == "Localized Label" &&
					choiceControl &&
					choiceControl->options.size() == 2 &&
					choiceControl->options[0].value == "$OPTION" &&
					choiceControl->options[0].label == "Localized Option" &&
					std::get<std::string>(choice.defaultValue) == "$OPTION",
				"choice localization changed its stored value or default");

			const auto& action =
				std::get<SendEventAction>(*RowNamed(root, "action").action);
			require(action.event == "$EVENT" &&
					std::get<std::string>(action.arguments.front()) == "$ARG",
				"localization changed raw action arguments");
			const auto& rich = SettingNamed(root, "rich");
			const auto& literal = SettingNamed(root, "literal");
			require(std::get<std::string>(rich.defaultValue) ==
						"Prefix Localized Option" &&
					std::get<std::string>(literal.defaultValue) ==
						"<b>Prefix</b> $OPTION",
				"prose HTML opt-in or literal markup mapping changed");
			require(rich.presentation.labelMode ==
						dmui::RowPresentation::LabelMode::kHidden &&
					rich.presentation.layout ==
						dmui::RowPresentation::Layout::kFullSpan,
				"prose row presentation stopped hiding and spanning its label");
		});

		runner.test("MCM localization misses are deduplicated and bounded", [] {
			const TextResolver missing =
				[](std::string_view) -> std::optional<std::string> {
					return std::nullopt;
				};
			const auto deduplicated = ParseConfig(R"json({
				"modName":"Missing",
				"displayName":"$MISSING",
				"content":[
					{"type":"section","text":"$MISSING"},
					{"id":"setting","type":"switch","text":"$MISSING","help":"$MISSING",
					 "valueOptions":{"sourceType":"ModSettingBool"}},
					{"id":"choice","type":"menu",
					 "valueOptions":{"sourceType":"ModSettingInt",
					 "options":["$MISSING"]}}
				]
			})json", "missing-localization.json", missing);
			require(DiagnosticCount(
						deduplicated,
						"localization key not found: $MISSING") == 1 &&
					deduplicated.pages.front().displayName == "$MISSING" &&
					SettingNamed(
						deduplicated.pages.front(),
						"setting").label == "$MISSING",
				"a missing localization key was hidden, guessed, or repeated");

			std::string json =
				R"({"modName":"Bounded","displayName":"Bounded","content":[)";
			for (size_t index = 0; index < 40; ++index)
			{
				if (index != 0)
					json.push_back(',');
				json += R"({"id":"text)" + std::to_string(index) +
					R"(","type":"text","text":"$KEY_)" +
					std::to_string(index) + R"("})";
			}
			json += "]}";
			const auto bounded =
				ParseConfig(json, "bounded-localization.json", missing);
			require(DiagnosticCount(
						bounded,
						"localization key not found:") == 32 &&
					DiagnosticCount(
						bounded,
						"additional missing localization keys omitted") == 1,
				"missing localization diagnostics were not bounded per config");
		});

		runner.test("MCM failed text resolvers preserve tokens", [] {
			const TextResolver failed =
				[](std::string_view) -> std::optional<std::string> {
					throw std::runtime_error("resolver offline");
				};
			const auto failure = ParseConfig(R"json({
				"modName":"Failure","displayName":"$TITLE",
				"content":[{"id":"text","type":"text","text":"$BODY"}]
			})json", "failed-resolver.json", failed);
			require(ErrorCount(failure) == 1 &&
					HasDiagnostic(
						failure,
						"text resolver failed; localization keys were preserved") &&
					failure.pages.front().displayName == "$TITLE" &&
					std::get<std::string>(
						SettingNamed(failure.pages.front(), "text").defaultValue) ==
						"$BODY",
				"a failed resolver discarded tokens or masqueraded as missing keys");
		});

	}
}
