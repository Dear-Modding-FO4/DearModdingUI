#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/TextMarkup.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "Harness.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI::MCM;

		constexpr std::string_view kSyntheticConfig = R"json({
			"minMcmVersion": 3,
			"modName": "ExampleMod",
			"displayName": "$EXAMPLE_MENU",
			"pluginRequirements": ["ExampleCore.esm", "SampleWorld.esp"],
			"content": [
				{"id":"introduction","type":"section","text":"$EXAMPLE_ROOT_SECTION"},
				{"id":"WelcomeMessage","type":"text","text":"$EXAMPLE_WELCOME",
				 "help":"$EXAMPLE_WELCOME_HELP"},
				{"id":"introduction","type":"section","text":"$EXAMPLE_ROOT_SECTION"},
				{"id":"OpenGuide","type":"button","text":"Open sample guide"}
			],
			"pages": [
				{
					"id": "controls",
					"pageDisplayName": "$EXAMPLE_CONTROLS",
					"content": [
						{"id":"basics","type":"section","text":"$EXAMPLE_BASICS"},
						{"id":"EnableFeature","type":"switcher",
						 "text":"$EXAMPLE_ENABLE","help":"$EXAMPLE_ENABLE_HELP",
						 "valueOptions":{
							"sourceType":"PropertyValueInt",
							"sourceForm":"ExampleCore.esm|100",
							"scriptName":"ExampleMod:Settings",
							"propertyName":"EnableFeature",
							"default":1
						 }},
						{"id":"DisplayMode","type":"dropdown",
						 "text":"$EXAMPLE_MODE","help":"$EXAMPLE_MODE_HELP",
						 "groupCondition":{"AND":[1,2]},
						 "valueOptions":{
							"sourceType":"PropertyValueInt",
							"sourceForm":"ExampleCore.esm|101",
							"scriptName":"ExampleMod:Settings",
							"propertyName":"DisplayMode",
							"default":0,
							"options":[
								"$EXAMPLE_MODE_CALM",
								"$EXAMPLE_MODE_BRIGHT",
								"$EXAMPLE_MODE_FOCUSED"
							]
						 }},
						{"id":"fSensitivity:SampleTweaks","type":"slider",
						 "text":"$EXAMPLE_SENSITIVITY","help":"$EXAMPLE_SENSITIVITY_HELP",
						 "valueOptions":{
							"sourceType":"ModSettingFloat",
							"default":1.0,
							"min":0.25,
							"max":2.5,
							"step":0.05
						 }},
						{"id":"iRetryCount:SampleTweaks","type":"slider",
						 "text":"Retry count","help":"Number of attempts",
						 "valueOptions":{
							"sourceType":"ModSettingInt",
							"default":3,
							"min":1,
							"max":8,
							"step":1
						 }},
						{"id":"extras","type":"section","text":"Extra controls"},
						{"id":"BindKey","type":"keymap","text":"Choose shortcut"},
						{"id":"AccentColor","type":"color","text":"Choose accent"},
						{"id":"PreviewImage","type":"image","text":"Sample preview"},
						{"id":"RunAction","type":"button","text":"Run sample action"}
					]
				},
				{
					"id": "sources",
					"displayName": "$EXAMPLE_SOURCES",
					"content": [
						{"id":"global-values","type":"section","text":"Global values"},
						{"id":"WorldScale","type":"slider",
						 "text":"World scale","help":"Scales the sample world",
						 "valueOptions":{
							"sourceType":"GlobalValue",
							"sourceForm":"SampleWorld.esp|200",
							"default":1.0,
							"min":0.0,
							"max":4.0,
							"step":0.1
						 }},
						{"id":"internal-values","type":"section","text":"Internal values"},
						{"id":"InternalState","type":"hidden",
						 "valueOptions":{
							"sourceType":"PropertyValueBool",
							"sourceForm":"ExampleCore.esm|102",
							"scriptName":"ExampleMod:State",
							"propertyName":"InternalState"
						 }}
					]
				}
			]
		})json";

		[[nodiscard]] std::filesystem::path TemporaryConfigPath(
			std::string_view a_name)
		{
			const auto nonce = std::chrono::steady_clock::now()
				.time_since_epoch()
				.count();
			return std::filesystem::temp_directory_path() /
				("dmui-mcm-" + std::string{ a_name } + "-" +
					std::to_string(nonce) + ".json");
		}

		struct TemporaryFileCleanup
		{
			std::filesystem::path path;

			~TemporaryFileCleanup()
			{
				std::error_code error;
				std::filesystem::remove(path, error);
			}
		};

		[[nodiscard]] const MappedPage& PageNamed(
			const LoadResult& a_result,
			std::string_view a_name)
		{
			const auto page = std::ranges::find(
				a_result.pages,
				a_name,
				&MappedPage::displayName);
			require(page != a_result.pages.end(),
				"mapped page was not found: " + std::string{ a_name });
			return *page;
		}

		[[nodiscard]] const dmui::SettingDescriptor& SettingNamed(
			const MappedPage& a_page,
			std::string_view a_id)
		{
			for (const auto& group : a_page.settings.groups)
			{
				const auto setting = std::ranges::find(
					group.settings,
					a_id,
					&dmui::SettingDescriptor::id);
				if (setting != group.settings.end())
					return *setting;
			}
			throw Failure("mapped setting was not found: " + std::string{ a_id });
		}

		[[nodiscard]] const MappedRow& RowNamed(
			const MappedPage& a_page,
			std::string_view a_id)
		{
			const auto row = std::ranges::find(
				a_page.rows,
				a_id,
				&MappedRow::id);
			require(row != a_page.rows.end(),
				"mapped row was not found: " + std::string{ a_id });
			return *row;
		}

		[[nodiscard]] const Control& ControlNamed(
			const Page& a_page,
			std::string_view a_id)
		{
			const auto control = std::ranges::find(
				a_page.controls,
				a_id,
				&Control::id);
			require(control != a_page.controls.end(),
				"declared control was not found: " + std::string{ a_id });
			return *control;
		}

		[[nodiscard]] const GroupCondition& ConditionNamed(
			const Page& a_page,
			std::string_view a_id)
		{
			const auto& control = ControlNamed(a_page, a_id);
			require(control.groupCondition.has_value(),
				"group condition was not retained: " + std::string{ a_id });
			return *control.groupCondition;
		}

		[[nodiscard]] size_t DescriptorCount(const MappedPage& a_page)
		{
			auto count = size_t{};
			for (const auto& group : a_page.settings.groups)
				count += group.settings.size();
			return count;
		}

		[[nodiscard]] size_t ControlKindCount(
			const LoadResult& a_result,
			dmui::SettingControlKind a_kind)
		{
			auto count = size_t{};
			for (const auto& page : a_result.pages)
			{
				for (const auto& group : page.settings.groups)
				{
					for (const auto& setting : group.settings)
					{
						if (dmui::ResolveSettingControlPresentation(
								setting.control).kind == a_kind)
							++count;
					}
				}
			}
			return count;
		}

		[[nodiscard]] bool HasDiagnostic(
			const LoadResult& a_result,
			std::string_view a_message,
			std::string_view a_location = {})
		{
			return std::ranges::any_of(
				a_result.diagnostics,
				[&](const Diagnostic& a_diagnostic) {
					return a_diagnostic.message.find(a_message) !=
							std::string::npos &&
						(a_location.empty() ||
							a_diagnostic.location.find(a_location) !=
								std::string::npos);
				});
		}

		[[nodiscard]] size_t ErrorCount(const LoadResult& a_result)
		{
			return static_cast<size_t>(std::ranges::count(
				a_result.diagnostics,
				DiagnosticSeverity::kError,
				&Diagnostic::severity));
		}

		[[nodiscard]] size_t DiagnosticCount(
			const LoadResult& a_result,
			std::string_view a_message)
		{
			return static_cast<size_t>(std::ranges::count_if(
				a_result.diagnostics,
				[&](const Diagnostic& a_diagnostic) {
					return a_diagnostic.message.find(a_message) !=
						std::string::npos;
				}));
		}

		[[nodiscard]] std::string ErrorMessages(const LoadResult& a_result)
		{
			std::string result;
			for (const auto& diagnostic : a_result.diagnostics)
			{
				if (diagnostic.severity != DiagnosticSeverity::kError)
					continue;
				if (!result.empty())
					result += "; ";
				result += diagnostic.location + ": " + diagnostic.message;
			}
			return result;
		}

		void RequireNear(double a_actual, double a_expected)
		{
			require(std::abs(a_actual - a_expected) < 0.000001,
				"numeric value did not match");
		}
	}

	void run_mcm_checks(Runner& runner)
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

		runner.test("MCM synthetic config preserves page and group structure", [] {
			const auto result = ParseConfig(
				kSyntheticConfig,
				"synthetic-config.json");
			require(result.configuration.has_value(),
				"synthetic configuration did not parse");
			require(ErrorCount(result) == 0,
				"synthetic configuration produced parse errors: " +
					ErrorMessages(result));
			require(result.configuration->modName == "ExampleMod" &&
					result.configuration->minimumMcmVersion ==
						std::optional<int64_t>{ 3 } &&
					result.configuration->displayName == "$EXAMPLE_MENU" &&
					result.configuration->pluginRequirements ==
						std::vector<std::string>{
							"ExampleCore.esm",
							"SampleWorld.esp" },
				"synthetic top-level metadata changed");
			require(result.pages.size() == 3 &&
					result.configuration->pages.front().root &&
					result.configuration->pages.front().id == "main",
				"synthetic root or page structure changed");

			const std::array<size_t, 3> groups{ 2, 2, 2 };
			const std::array<size_t, 3> settings{ 1, 6, 1 };
			for (size_t index = 0; index < result.pages.size(); ++index)
			{
				require(result.pages[index].settings.groups.size() == groups[index],
					"synthetic group shape changed");
				require(DescriptorCount(result.pages[index]) == settings[index],
					"synthetic descriptor shape changed");
				std::unordered_set<std::string> ids;
				for (const auto& group : result.pages[index].settings.groups)
					require(ids.insert(group.id).second,
						"duplicate group id survived mapping");
			}
			require(
				result.pages.front().settings.groups.front().label ==
					"$EXAMPLE_ROOT_SECTION",
				"synthetic root section label changed");
			require(result.pages.front().settings.groups[0].id ==
						"introduction" &&
					result.pages.front().settings.groups[1].id ==
						"introduction-2",
				"duplicate synthetic group ids were not uniqued");
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

		runner.test("MCM LoadConfig reads a synthetic temporary file", [] {
			const auto path = TemporaryConfigPath("load-success");
			const TemporaryFileCleanup cleanup{ path };
			{
				std::ofstream file(path, std::ios::binary);
				require(file.is_open(),
					"temporary MCM configuration could not be created");
				file << kSyntheticConfig;
				require(file.good(),
					"temporary MCM configuration could not be written");
			}

			const auto result = LoadConfig(path);
			require(result.configuration &&
					result.configuration->modName == "ExampleMod" &&
					result.pages.size() == 3 &&
					ErrorCount(result) == 0,
				"temporary MCM configuration did not load");
			const auto localized = LoadConfig(
				path,
				[](std::string_view a_key) -> std::optional<std::string> {
					return a_key == "$EXAMPLE_MENU" ?
						std::optional<std::string>{ "Localized file title" } :
						std::optional<std::string>{ std::string{ a_key } };
				});
			require(localized.pages.front().displayName ==
						"Localized file title" &&
					localized.configuration->displayName == "$EXAMPLE_MENU",
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

		runner.test("MCM unknown controls degrade to unsupported", [] {
			const auto result = ParseConfig(R"({
				"minMcmVersion": 2,
				"modName": "Future",
				"displayName": "Future",
				"pluginRequirements": [],
				"content": [
					{"id":"future","text":"Future","help":"New control","type":"dial"}
				]
			})", "future-config.json");
			require(result.configuration && result.pages.size() == 1,
				"future control prevented its page from mapping");
			const auto& setting = SettingNamed(result.pages.front(), "future");
			require(std::holds_alternative<dmui::UnsupportedSettingControl>(
						setting.control),
				"future control did not degrade to unsupported");
			require(result.configuration->pages.front().controls.front().rawType ==
					"dial",
				"future control type was not retained in the IR");
			require(HasDiagnostic(result, "unknown MCM control type",
						"$.content[0]"),
				"future control was not diagnosed");
		});

		runner.test("MCM empty and malformed pages retain diagnostics", [] {
			for (const auto content : { "[]", "[17]", R"([{"type":"section","text":"Empty"}])" })
			{
				const auto result = ParseConfig(
					std::string{ R"({"modName":"Empty","displayName":"Empty","content":)" } +
						content + "}");
				require(result.pages.size() == 1 &&
						DiagnosticCount(result, "page produced no setting descriptors") == 1 &&
						result.pages.front().settings.notes.empty(),
					"an empty or malformed page lost its empty-content diagnostic");
				if (std::string_view{ content } == "[17]")
					require(ErrorCount(result) > 0,
						"malformed control lost its parser error");
			}
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
					{"id":"typeless","text":"Typeless"}
				]
			})", "incomplete-config.json");
			require(result.configuration && result.pages.size() == 1 &&
					DescriptorCount(result.pages.front()) == 3,
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
			require(HasDiagnostic(result, "missing valueOptions",
						"$.content[0]") &&
					HasDiagnostic(result, "missing required string",
						"$.content[2].type"),
				"incomplete controls were not fully diagnosed");
		});

		runner.test("MCM documented control vocabulary maps generically", [] {
			const auto result = ParseConfig(R"({
				"minMcmVersion": 2,
				"modName": "Vocabulary",
				"displayName": "Vocabulary",
				"content": [
					{"type":"header","text":"All"},
					{"id":"switch","type":"switch","valueOptions":{"sourceType":"ModSettingBool"}},
					{"id":"stepper","type":"stepper","valueOptions":{"sourceType":"ModSettingInt","options":["A","B"]}},
					{"id":"menu","type":"menu","valueOptions":{"sourceType":"ModSettingInt","options":["A","B"]}},
					{"id":"enum","type":"enum","valueOptions":{"sourceType":"ModSettingInt","options":["A","B"]}},
					{"id":"input","type":"input","valueOptions":{"sourceType":"ModSettingString"}},
					{"id":"textinput","type":"textinput","valueOptions":{"sourceType":"ModSettingString"}},
					{"id":"text","type":"text","text":"Read only"},
					{"type":"empty"},
					{"id":"hidden","type":"hidden"},
					{"id":"button","type":"button"},
					{"id":"keymap","type":"keymap"},
					{"id":"color","type":"color"},
					{"id":"image","type":"image"}
				]
			})", "vocabulary-config.json");
			require(result.configuration && result.pages.size() == 1 &&
					result.pages.front().settings.groups.size() == 1 &&
					DescriptorCount(result.pages.front()) == 9,
				"documented control structure did not map");
			require(ControlKindCount(
						result,
						dmui::SettingControlKind::kCheckbox) == 1 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kChoice) == 3 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kText) == 2 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kReadOnly) == 2 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kUnsupported) == 1,
				"documented control kinds changed");
			require(std::holds_alternative<dmui::TextSettingControl>(
						SettingNamed(result.pages.front(), "input").control) &&
					std::holds_alternative<dmui::TextSettingControl>(
						SettingNamed(result.pages.front(), "textinput").control),
				"an input spelling degraded to unsupported");
			const auto& prose =
				SettingNamed(result.pages.front(), "text");
			require(prose.label.empty() &&
					prose.presentation.labelMode ==
						dmui::RowPresentation::LabelMode::kHidden &&
					prose.presentation.layout ==
						dmui::RowPresentation::Layout::kFullSpan,
				"MCM prose did not request a full-span hidden-label row");
			require(!std::ranges::any_of(
						result.pages.front().settings.groups.front().settings,
						[](const dmui::SettingDescriptor& a_setting) {
							return a_setting.id == "hidden";
						}),
				"hidden control was emitted");
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

		runner.test("MCM mapped bindings correlate descriptors to sources", [] {
			const auto result = ParseConfig(
				kSyntheticConfig,
				"synthetic-config.json");
			require(result.configuration.has_value(),
				"synthetic configuration did not parse");

			const auto binding = [&](
				const MappedPage& a_page,
				std::string_view a_id) -> const MappedBinding& {
				const auto found = std::ranges::find(
					a_page.rows,
					a_id,
					&MappedRow::id);
				require(found != a_page.rows.end() && found->binding,
					"binding not found for descriptor: " + std::string{ a_id });
				return *found->binding;
			};

			// Every binding descriptorId must resolve to a real descriptor on
			// the same page, so phase 2 never re-derives uniquified ids.
			for (const auto& page : result.pages)
			{
				for (const auto& row : page.rows)
				{
					if (!row.binding || !row.emitted)
						continue;
					[[maybe_unused]] const auto& descriptor =
						SettingNamed(page, row.binding->descriptorId);
				}
			}

			const auto& controls = PageNamed(result, "$EXAMPLE_CONTROLS");
			const auto& property = binding(controls, "DisplayMode");
			const auto* propertySource =
				std::get_if<PropertyBinding>(&property.source);
			require(property.Family() == SourceFamily::kProperty &&
					property.valueKind == SourceValueKind::kInt &&
					propertySource &&
					propertySource->propertyName == "DisplayMode" &&
					propertySource->scriptName ==
						std::optional<std::string>{ "ExampleMod:Settings" } &&
					propertySource->form == "ExampleCore.esm|101",
				"property binding lost its resolved source");

			const auto& modSetting =
				binding(controls, "fSensitivity:SampleTweaks");
			const auto* modSettingSource =
				std::get_if<ModSettingBinding>(&modSetting.source);
			require(modSetting.Family() == SourceFamily::kModSetting &&
					modSettingSource &&
					modSettingSource->key == "fSensitivity" &&
					modSettingSource->section == "SampleTweaks" &&
					modSettingSource->declaration == DeclarationState::kUnknown,
				"mod setting binding lost its id");

			const auto& sources = PageNamed(result, "$EXAMPLE_SOURCES");
			const auto& global = binding(sources, "WorldScale");
			const auto* globalSource =
				std::get_if<GlobalBinding>(&global.source);
			require(global.Family() == SourceFamily::kGlobal &&
					globalSource &&
					globalSource->form == "SampleWorld.esp|200",
				"global binding lost its source form");
		});

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
