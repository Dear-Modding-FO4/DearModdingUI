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
				if (a_key == "$HEADING")
					return "Localized Heading";
				if (a_key == "$LABEL")
					return "Localized Label";
				if (a_key == "$HELP")
					return "Localized Help";
				if (a_key == "$BODY")
					return "Localized Body";
				if (a_key == "$BODY_TWO")
					return "Localized Second";
				if (a_key == "$BODY suffix")
					return "Whole key with spaces";
				if (a_key == "$CHOICE_LABEL")
					return "Localized Choice";
				if (a_key == "$OPTION_A")
					return "Localized Option";
				if (a_key == "$BUTTON")
					return "Localized Button";
				if (a_key == "$BUTTON_HELP")
					return "Localized Button Help";
				if (a_key == "$PAGE")
					return "Localized Page";
				return std::nullopt;
			};
			constexpr auto json = R"json({
				"modName":"IdentityMod",
				"displayName":"$CLIENT NAME",
				"content":[
					{"type":"section","text":"<b>$HEADING</b>","html":true},
					{"id":"bEnabled:Main","type":"switcher",
					 "text":"$LABEL","help":"$HELP",
					 "valueOptions":{"sourceType":"ModSettingBool","default":true}},
					{"id":"read","type":"text","html":true,
					 "text":"<p>$BODY</p><br>$BODY_TWO"},
					{"id":"sMode:Main","type":"dropdown","text":"$CHOICE_LABEL",
					 "valueOptions":{"sourceType":"ModSettingString",
					 "default":"$OPTION_A","options":["$OPTION_A","literal"]}},
					{"id":"action","type":"button","text":"$BUTTON","help":"$BUTTON_HELP",
					 "action":{"type":"SendEvent","event":"$EVENT","params":["$ARG"]}},
					{"id":"literal","type":"text","text":"Prefix $BODY"},
					{"id":"spacedKey","type":"text","text":"$BODY suffix"}
				],
				"pages":[{
					"pageDisplayName":"$PAGE",
					"content":[{"id":"input","type":"input","text":"",
						"valueOptions":{"sourceType":"ModSettingString",
						"default":"$PERSISTED"}}]
				}]
			})json";
			const auto result = ParseConfig(json, "localized.json", resolver);
			const auto untranslated = ParseConfig(json, "localized.json");
			require(result.configuration && result.pages.size() == 2 &&
					untranslated.configuration && untranslated.pages.size() == 2 &&
					result.diagnostics.empty(),
				"localized fixture did not map cleanly");
			require(result.configuration->modName == "IdentityMod" &&
					result.configuration->displayName == "$CLIENT NAME" &&
					result.displayName == "Localized Client" &&
					result.configuration->pages[1].id ==
						untranslated.configuration->pages[1].id &&
					result.pages[0].id == untranslated.pages[0].id &&
					result.pages[1].id == untranslated.pages[1].id,
				"localized presentation changed raw configuration identity");

			const auto& root = result.pages[0];
			require(root.displayName == "Localized Client" &&
					root.settings.groups.front().id ==
						untranslated.pages[0].settings.groups.front().id &&
					root.settings.groups.front().label == "Localized Heading",
				"client or heading presentation was not localized");
			const auto& enabled = SettingNamed(root, "bEnabled:Main");
			require(enabled.label == "Localized Label" &&
					enabled.description == "Localized Help",
				"control label or help was not localized");
			const auto& read = SettingNamed(root, "read");
			require(std::get<std::string>(read.defaultValue) ==
						"Localized Body\nLocalized Second" &&
					RowNamed(root, "read").text &&
					RowNamed(root, "read").text->presentation.text ==
						"Localized Body\nLocalized Second",
				"HTML read-only text was not localized before presentation mapping");
			require(std::get<std::string>(
						SettingNamed(root, "literal").defaultValue) ==
						"Prefix $BODY" &&
					std::get<std::string>(
						SettingNamed(root, "spacedKey").defaultValue) ==
						"Whole key with spaces",
				"localization did not distinguish exact keys from embedded tokens");

			const auto& choice = SettingNamed(root, "sMode:Main");
			const auto* choiceControl =
				std::get_if<dmui::ChoiceSettingControl>(&choice.control);
			require(choiceControl && choiceControl->options.size() == 2 &&
					choiceControl->options[0].value == "$OPTION_A" &&
					choiceControl->options[0].label == "Localized Option" &&
					choiceControl->options[1].value == "literal" &&
					choiceControl->options[1].label == "literal" &&
					std::get<std::string>(choice.defaultValue) == "$OPTION_A",
				"choice localization changed its stored value or default");

			const auto& action =
				std::get<SendEventAction>(*RowNamed(root, "action").action);
			require(root.settings.groups.front().actionRows.front().buttonLabel ==
						"Localized Button" &&
					root.settings.groups.front().actionRows.front().description ==
						"Localized Button Help" &&
					action.event == "$EVENT" &&
					std::get<std::string>(action.arguments.front()) == "$ARG",
				"button presentation or raw action arguments changed");
			require(result.pages[1].displayName == "Localized Page" &&
					std::get<std::string>(
						SettingNamed(result.pages[1], "input").defaultValue) ==
						"$PERSISTED",
				"page localization changed its identity or persisted input");

			const auto fallback = ParseConfig(R"json({
				"modName":"Literal fallback",
				"displayName":"",
				"content":[{"id":"empty","type":"text","text":""}]
			})json", "literal-fallback.json", resolver);
			require(fallback.configuration &&
					fallback.diagnostics.empty() &&
					fallback.configuration->displayName.empty() &&
					fallback.displayName == "Literal fallback" &&
					fallback.pages.front().displayName == "Literal fallback" &&
					std::get<std::string>(
						SettingNamed(fallback.pages.front(), "empty").defaultValue)
						.empty(),
				"literal or empty display text changed during localization");

			const auto pagesOnly = ParseConfig(R"json({
				"modName":"IndependentName","displayName":"$CLIENT NAME",
				"pages":[{"pageDisplayName":"$PAGE","content":[
					{"type":"text","text":"Page content"}
				]}]
			})json", "pages-only.json", resolver);
			require(pagesOnly.configuration && pagesOnly.pages.size() == 1 &&
					pagesOnly.displayName == "Localized Client" &&
					pagesOnly.pages.front().displayName == "Localized Page" &&
					HasDiagnostic(pagesOnly, "missing required content array"),
				"a missing root page replaced the client name with a page label");
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

		runner.test("MCM absent and failed text resolvers preserve tokens", [] {
			const auto absent = ParseConfig(R"json({
				"modName":"Absent","displayName":"$TITLE",
				"content":[{"id":"text","type":"text","text":"$BODY"}]
			})json", "absent-resolver.json");
			require(absent.diagnostics.empty() &&
					absent.displayName == "$TITLE" &&
					absent.pages.front().displayName == "$TITLE" &&
					std::get<std::string>(
						SettingNamed(absent.pages.front(), "text").defaultValue) ==
						"$BODY",
				"default callers did not retain the pre-localization behavior");

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
