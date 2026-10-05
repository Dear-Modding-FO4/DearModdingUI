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
