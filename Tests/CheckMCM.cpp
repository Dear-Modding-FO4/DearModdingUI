#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/JsonNormalization.h>
#include <DearModdingUI/MCM/TextMarkup.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include "Harness.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
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

		[[nodiscard]] const Page& DeclaredPageNamed(
			const Configuration& a_configuration,
			std::string_view a_name)
		{
			const auto page = std::ranges::find(
				a_configuration.pages,
				a_name,
				&Page::displayName);
			require(page != a_configuration.pages.end(),
				"declared page was not found: " + std::string{ a_name });
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
		runner.test("MCM HTML text strips tags and expands break forms", [] {
			const auto presentation = ResolveTextPresentation(
				"A<br>B<br/>C<br />D <i>I</i> <b>B</b> <u>U</u> "
				"<a href='target'>A</a> <font size='30'>F</font> "
				"<unknown data='value'>U</unknown>",
				true);
			require(
				presentation.text == "A\nB\nC\nD I B U A F U",
				"HTML tag stripping or break expansion changed");
		});

		runner.test("MCM HTML paragraphs separate text and resolve alignment", [] {
			const auto presentation = ResolveTextPresentation(
				"Before<p>Plain</p><p ALIGN='right'>Right</p>After",
				true);
			require(
				presentation.text == "Before\nPlain\nRight\nAfter" &&
					presentation.alignment == TextAlignment::kRight,
				"paragraph text or alignment did not resolve");
		});

		runner.test("MCM literal text preserves markup when HTML is disabled", [] {
			const auto presentation = ResolveTextPresentation(
				"<Press E> <i>literal</i> &amp;",
				false,
				"center");
			require(
				presentation.text == "<Press E> <i>literal</i> &amp;" &&
					presentation.alignment == TextAlignment::kCenter,
				"literal markup or control alignment changed");
		});

		runner.test("MCM malformed HTML remains readable", [] {
			const auto presentation = ResolveTextPresentation(
				"Keep < stray <i>open</i> <font size='30' broken &bogus; tail",
				true);
			require(
				presentation.text ==
					"Keep < stray open <font size='30' broken &bogus; tail",
				"malformed HTML lost readable source text");
		});

		runner.test("MCM HTML decodes common and numeric entities", [] {
			const auto presentation = ResolveTextPresentation(
				"&lt;safe&gt; &amp; &quot;text&quot; &apos;x&apos; "
				"&#65;&#x42;",
				true);
			require(
				presentation.text == "<safe> & \"text\" 'x' AB",
				"HTML entity decoding changed");
		});

		runner.test("MCM paragraph alignment overrides the control alignment", [] {
			const auto presentation = ResolveTextPresentation(
				"<p align=\"center\">Centered</p>",
				true,
				"right");
			require(
				presentation.text == "Centered" &&
					presentation.alignment == TextAlignment::kCenter,
				"paragraph alignment did not override the control default");
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
					numeric->quantization,
				"float slider range was not mapped");
			RequireNear(*numeric->range->minimum, 0.25);
			RequireNear(*numeric->range->maximum, 2.5);
			RequireNear(numeric->quantization->interval, 0.05);
			RequireNear(numeric->quantization->origin, 0.25);
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
					integer->quantization &&
					integer->quantization->interval == 1 &&
					integer->quantization->origin == 1,
				"integer slider did not retain its range and quantization");
			require(ControlKindCount(
						result,
						dmui::SettingControlKind::kDouble) == 2 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kSigned) == 1,
				"synthetic numeric control kinds changed");
		});

		runner.test("MCM synthetic config retains source and condition metadata", [] {
			const auto result = ParseConfig(
				kSyntheticConfig,
				"synthetic-config.json");
			const auto& controls = DeclaredPageNamed(
				*result.configuration,
				"$EXAMPLE_CONTROLS");
			const auto& property = ControlNamed(controls, "DisplayMode");
			require(property.valueOptions &&
					property.valueOptions->sourceType &&
					property.valueOptions->sourceType->raw ==
						"PropertyValueInt" &&
					property.valueOptions->sourceType->family ==
						SourceFamily::kProperty &&
					property.valueOptions->sourceType->value ==
						SourceValueKind::kInt &&
					property.valueOptions->sourceForm ==
						std::optional<std::string>{ "ExampleCore.esm|101" } &&
					property.valueOptions->propertyName ==
						std::optional<std::string>{ "DisplayMode" } &&
					property.valueOptions->scriptName ==
						std::optional<std::string>{ "ExampleMod:Settings" },
				"property source metadata did not survive");
			require(property.groupCondition &&
					property.groupCondition->type == ConditionType::kAll &&
					property.groupCondition->operands.size() == 2,
				"compound group condition did not survive");

			const auto& modSetting = ControlNamed(
				controls,
				"fSensitivity:SampleTweaks");
			require(modSetting.valueOptions &&
					modSetting.valueOptions->sourceType &&
					modSetting.valueOptions->sourceType->raw ==
						"ModSettingFloat" &&
					modSetting.valueOptions->sourceType->family ==
						SourceFamily::kModSetting &&
					modSetting.valueOptions->sourceType->value ==
						SourceValueKind::kFloat &&
					modSetting.valueOptions->modSettingId ==
						std::optional<std::string>{
							"fSensitivity:SampleTweaks" },
				"ModSetting source metadata did not survive");

			const auto& sources = DeclaredPageNamed(
				*result.configuration,
				"$EXAMPLE_SOURCES");
			const auto& global = ControlNamed(sources, "WorldScale");
			require(global.valueOptions &&
					global.valueOptions->sourceType &&
					global.valueOptions->sourceType->raw == "GlobalValue" &&
					global.valueOptions->sourceType->family ==
						SourceFamily::kGlobal &&
					global.valueOptions->sourceType->value ==
						SourceValueKind::kNone &&
					global.valueOptions->sourceForm ==
						std::optional<std::string>{ "SampleWorld.esp|200" },
				"global source metadata did not survive");
			const auto& hidden = ControlNamed(sources, "InternalState");
			require(hidden.type == ControlType::kHidden,
				"hidden control type did not survive");
			require(hidden.valueOptions.has_value(),
				"hidden value options did not survive");
			require(hidden.valueOptions->sourceForm &&
					*hidden.valueOptions->sourceForm == "ExampleCore.esm|102",
				"hidden source form did not survive");
			require(hidden.valueOptions->scriptName ==
						std::optional<std::string>{ "ExampleMod:State" },
				"hidden script name did not survive");
			require(hidden.valueOptions->propertyName ==
						std::optional<std::string>{ "InternalState" },
				"hidden property name did not survive");
		});

		runner.test("MCM numeric steps without minima use a zero origin", [] {
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
			require(floating && floating->quantization &&
					floating->quantization->interval == 0.25 &&
					floating->quantization->origin == 0.0 &&
					integer && integer->quantization &&
					integer->quantization->interval == 2 &&
					integer->quantization->origin == 0,
				"a step without a minimum was silently discarded");
		});

		runner.test("MCM synthetic config maps readable and unsupported controls", [] {
			const auto result = ParseConfig(
				kSyntheticConfig,
				"synthetic-config.json");
			const auto& root = PageNamed(result, "$EXAMPLE_MENU");
			require(dmui::ResolveSettingControlPresentation(
						SettingNamed(root, "WelcomeMessage").control).kind ==
						dmui::SettingControlKind::kReadOnly,
				"text did not map to read-only");
			require(!std::get<dmui::ReadOnlySettingControl>(
						SettingNamed(root, "WelcomeMessage").control).draw,
				"mapper attached consumer-specific text rendering");
			const auto mappedText = std::ranges::find(
				root.rows,
				"WelcomeMessage",
				&MappedRow::id);
			require(mappedText != root.rows.end() && mappedText->text &&
					mappedText->text->presentation.text == "$EXAMPLE_WELCOME",
				"text presentation data was not mapped");
			require(SettingNamed(root, "WelcomeMessage").label.empty() &&
					SettingNamed(root, "WelcomeMessage")
							.presentation.labelMode ==
						dmui::RowPresentation::LabelMode::kHidden &&
					SettingNamed(root, "WelcomeMessage")
							.presentation.layout ==
						dmui::RowPresentation::Layout::kFullSpan,
				"text control did not request a full-span prose row");
			require(ControlKindCount(
						result,
						dmui::SettingControlKind::kUnsupported) == 1,
				"color control did not map to unsupported");
		});

		runner.test("MCM mapper gates markup on the HTML declaration", [] {
			const auto result = ParseConfig(R"({
				"modName":"Markup",
				"content":[
					{"id":"rich","type":"text","html":true,"align":"right",
					 "text":"<p align='center'><i>Rich</i><br />text</p>"},
					{"id":"literal","type":"text","html":false,
					 "text":"Literal <Press E>"}
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
						"Literal <Press E>",
				"mapper did not honor the HTML declaration");
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
			require(std::holds_alternative<dmui::UnsupportedSettingControl>(
						SettingNamed(result.pages.front(), "typeless").control),
				"typeless control did not degrade to unsupported");
			require(HasDiagnostic(result, "missing valueOptions",
						"$.content[0]") &&
					HasDiagnostic(result, "slider has no numeric range",
						"$.content[1]") &&
					HasDiagnostic(result, "missing required string",
						"$.content[2].type"),
				"incomplete controls were not fully diagnosed");
		});

		runner.test("MCM absent files return an empty diagnosed result", [] {
			const auto path = TemporaryConfigPath("does-not-exist");
			const auto result = LoadConfig(path);
			require(!result.configuration && result.pages.empty(),
				"absent file produced configuration data");
			require(HasDiagnostic(result, "could not open"),
				"absent file was not diagnosed");
			require(result.diagnostics.front().source.find(
						"does-not-exist") != std::string::npos,
				"absent file diagnostic lost its path");
		});

		runner.test("MCM IR retains defaults formats options and property fields", [] {
			const auto result = ParseConfig(R"({
				"minMcmVersion": 2,
				"modName": "Metadata",
				"displayName": "Metadata",
				"content": [
					{"type":"section","text":"Settings"},
					{"id":"FloatValue","text":"Float","help":"Float help","type":"slider",
					 "valueOptions":{
						"sourceType":"PropertyValueFloat",
						"sourceForm":"Metadata.esp|123",
						"scriptName":"Metadata:Settings",
						"propertyName":"FloatValue",
						"default":1.25,
						"min":0,
						"max":2,
						"step":0.25,
						"format":"%.2f"
					 }},
					{"id":"sMode:General","text":"Mode","type":"menu",
					 "valueOptions":{
						"sourceType":"ModSettingString",
						"default":"careful",
						"options":["fast","careful"]
					 }}
				]
			})", "metadata-config.json");
			require(result.configuration && ErrorCount(result) == 0,
				"metadata configuration did not parse");
			const auto& declared =
				result.configuration->pages.front().controls[1];
			require(declared.valueOptions &&
					declared.valueOptions->sourceType &&
					declared.valueOptions->sourceType->raw ==
						"PropertyValueFloat" &&
					declared.valueOptions->sourceType->family ==
						SourceFamily::kProperty &&
					declared.valueOptions->sourceType->value ==
						SourceValueKind::kFloat &&
					declared.valueOptions->sourceForm ==
						std::optional<std::string>{ "Metadata.esp|123" } &&
					declared.valueOptions->scriptName ==
						std::optional<std::string>{ "Metadata:Settings" } &&
					declared.valueOptions->propertyName ==
						std::optional<std::string>{ "FloatValue" } &&
					declared.valueOptions->defaultValue &&
					declared.valueOptions->minimum ==
						std::optional<double>{ 0.0 } &&
					declared.valueOptions->maximum ==
						std::optional<double>{ 2.0 } &&
					declared.valueOptions->step ==
						std::optional<double>{ 0.25 } &&
					declared.valueOptions->format ==
						std::optional<std::string>{ "%.2f" },
				"property metadata fields did not survive");
			require(std::get<double>(
						*declared.valueOptions->defaultValue) == 1.25,
				"numeric default changed");

			const auto& mapped = SettingNamed(
				result.pages.front(),
				"FloatValue");
			const auto* numeric =
				std::get_if<dmui::DoubleSettingControl>(&mapped.control);
			require(numeric && numeric->format == "%.2f" &&
					numeric->quantization &&
					numeric->quantization->interval == 0.25 &&
					numeric->quantization->origin == 0.0 &&
					std::get<double>(mapped.defaultValue) == 1.25,
				"numeric format, quantization, or default did not map");

			const auto& choiceDeclaration =
				result.configuration->pages.front().controls[2];
			require(choiceDeclaration.valueOptions &&
					choiceDeclaration.valueOptions->modSettingId ==
						std::optional<std::string>{ "sMode:General" } &&
					choiceDeclaration.valueOptions->options.size() == 2,
				"ModSetting options metadata did not survive");
			const auto& choice = SettingNamed(
				result.pages.front(),
				"sMode:General");
			const auto* control =
				std::get_if<dmui::ChoiceSettingControl>(&choice.control);
			require(control && control->options[1].value == "careful" &&
					std::get<std::string>(choice.defaultValue) == "careful",
				"string choice values did not map");
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
					DescriptorCount(result.pages.front()) == 8,
				"documented control structure did not map");
			require(ControlKindCount(
						result,
						dmui::SettingControlKind::kCheckbox) == 1 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kChoice) == 3 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kText) == 1 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kReadOnly) == 2 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kUnsupported) == 1,
				"documented control kinds changed");
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

		runner.test("MCM hotkeys map to read-only status rows", [] {
			const auto result = ParseConfig(R"({
				"modName": "Hotkeys",
				"displayName": "Hotkeys",
				"content": [
					{"id":"bare","type":"hotkey","text":"Bare","help":"Bare help"},
					{"id":"modified","type":"keymap","text":"Modified","help":"Modified help",
					 "valueOptions":{"allowModifierKeys":true}}
				]
			})", "hotkey-config.json");
			require(result.configuration && result.pages.size() == 1 &&
					DescriptorCount(result.pages.front()) == 2,
				"hotkey controls did not map");
			require(ControlKindCount(
						result,
						dmui::SettingControlKind::kReadOnly) == 2,
				"hotkey controls did not map to read-only");

			const auto& bare = SettingNamed(result.pages.front(), "bare");
			const auto& modified =
				SettingNamed(result.pages.front(), "modified");
			require(std::holds_alternative<dmui::ReadOnlySettingControl>(
						bare.control) &&
					std::holds_alternative<dmui::ReadOnlySettingControl>(
						modified.control),
				"a hotkey control degraded from read-only");
			require(!std::get<dmui::ReadOnlySettingControl>(bare.control).draw &&
					!std::get<dmui::ReadOnlySettingControl>(modified.control).draw &&
					std::ranges::count_if(
						result.pages.front().rows,
						[](const MappedRow& a_row) {
							return a_row.text.has_value();
						}) == 2,
				"mapper attached consumer-specific hotkey rendering");
			require(std::get<std::string>(bare.defaultValue) ==
						"Unbound" &&
					std::get<std::string>(modified.defaultValue) ==
						"Unbound",
				"hotkey status text changed");
			require(!bare.showReset && !modified.showReset,
				"hotkey controls exposed reset actions");
			require(!HasDiagnostic(result, "unsupported in this phase"),
				"hotkey controls emitted unsupported diagnostics");
		});

		runner.test("MCM empty sections map to unnamed groups", [] {
			const auto result = ParseConfig(R"({
				"modName": "EmptySection",
				"displayName": "Empty Section",
				"content": [
					{"id":"divider","type":"section","text":""},
					{"id":"enabled","type":"switcher"}
				]
			})", "empty-section-config.json");
			require(result.pages.size() == 1 &&
					result.pages.front().settings.groups.size() == 1,
				"empty section did not produce one group");
			const auto& group = result.pages.front().settings.groups.front();
			require(group.id == "divider" && group.label.empty() &&
					group.glyph == U'\0' &&
					group.headingMode ==
						dmui::SettingGroup::HeadingMode::kDivider,
				"empty section did not produce an unnamed group");
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
		});

		runner.test("MCM text input spellings both map to text controls", [] {
			const auto result = ParseConfig(R"({
				"modName": "Inputs",
				"displayName": "Inputs",
				"content": [
					{"id":"legacy","type":"input","valueOptions":{"sourceType":"ModSettingString"}},
					{"id":"shipped","type":"textinput","valueOptions":{"sourceType":"ModSettingString"}}
				]
			})", "input-config.json");
			require(result.configuration && result.pages.size() == 1 &&
					DescriptorCount(result.pages.front()) == 2,
				"text input controls did not map");
			require(std::holds_alternative<dmui::TextSettingControl>(
						SettingNamed(result.pages.front(), "legacy").control) &&
					std::holds_alternative<dmui::TextSettingControl>(
						SettingNamed(result.pages.front(), "shipped").control),
				"a text input spelling degraded to unsupported");
		});

		runner.test("MCM minimum version tolerates absence and decimals", [] {
			const auto absent = ParseConfig(R"({
				"modName": "Absent",
				"displayName": "Absent",
				"content": [{"id":"a","type":"switch",
					"valueOptions":{"sourceType":"ModSettingBool"}}]
			})", "absent-version.json");
			require(absent.configuration && ErrorCount(absent) == 0 &&
					!absent.configuration->minimumMcmVersion,
				"an omitted minimum version was treated as a failure");

			const auto decimal = ParseConfig(R"({
				"minMcmVersion": 1.10,
				"modName": "Decimal",
				"displayName": "Decimal",
				"content": [{"id":"a","type":"switch",
					"valueOptions":{"sourceType":"ModSettingBool"}}]
			})", "decimal-version.json");
			require(decimal.configuration && ErrorCount(decimal) == 0 &&
					decimal.configuration->minimumMcmVersion == 1,
				"a decimal minimum version was treated as a failure");

			const auto invalid = ParseConfig(R"({
				"minMcmVersion": "one",
				"modName": "Invalid",
				"displayName": "Invalid",
				"content": [{"id":"a","type":"switch",
					"valueOptions":{"sourceType":"ModSettingBool"}}]
			})", "invalid-version.json");
			require(ErrorCount(invalid) == 1 &&
					!invalid.configuration->minimumMcmVersion,
				"a non-numeric minimum version stopped being diagnosed");
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
					{"id":"g","type":"slider","valueOptions":{"sourceType":"GlobalValue","min":0,"max":1}},
					{"id":"pb","type":"switch","valueOptions":{"sourceType":"PropertyValueBool"}},
					{"id":"pi","type":"slider","valueOptions":{"sourceType":"PropertyValueInt","min":0,"max":1}},
					{"id":"pf","type":"slider","valueOptions":{"sourceType":"PropertyValueFloat","min":0,"max":1}},
					{"id":"ps","type":"input","valueOptions":{"sourceType":"PropertyValueString"}},
					{"id":"px","type":"slider","valueOptions":{"sourceType":"PropertyValueEx","min":0,"max":1}},
					{"id":"mb:S","type":"switch","valueOptions":{"sourceType":"ModSettingBool"}},
					{"id":"mi:S","type":"slider","valueOptions":{"sourceType":"ModSettingInt","min":0,"max":1}},
					{"id":"mf:S","type":"slider","valueOptions":{"sourceType":"ModSettingFloat","min":0,"max":1}},
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

		runner.test("MCM NOT group conditions stay outside FO4 scope", [] {
			const auto result = ParseConfig(R"json({
				"modName":"NotConditionFixture",
				"content":[
					{"id":"target","type":"text","groupCondition":{"NOT":1}}
				]
			})json", "not-condition.json");
			const auto& condition = ConditionNamed(
				result.configuration->pages.front(),
				"target");
			require(condition.type == ConditionType::kUnknown &&
					condition.rawOperator == "NOT" &&
					condition.operands.empty(),
				"NOT condition no longer has its current partial representation");
			require(HasDiagnostic(result, "unknown condition operator 'NOT'") &&
					HasDiagnostic(result, "condition operands must be an array"),
				"NOT condition diagnostics changed");
		});

		runner.test("MCM ONLY group conditions stay outside FO4 scope", [] {
			const auto result = ParseConfig(R"json({
				"modName":"OnlyConditionFixture",
				"content":[
					{"id":"target","type":"text","groupCondition":{"ONLY":[1,2]}}
				]
			})json", "only-condition.json");
			const auto& condition = ConditionNamed(
				result.configuration->pages.front(),
				"target");
			require(condition.type == ConditionType::kUnknown &&
					condition.rawOperator == "ONLY" &&
					condition.operands.size() == 2,
				"ONLY condition no longer has its current partial representation");
			require(HasDiagnostic(result, "unknown condition operator 'ONLY'"),
				"ONLY condition stopped producing its current diagnostic");
		});

		runner.test("MCM comparison conditions stay outside FO4 scope", [] {
			const auto result = ParseConfig(R"json({
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
			const auto& condition = ConditionNamed(
				result.configuration->pages.front(),
				"target");
			require(condition.type == ConditionType::kUnknown,
				"comparison object stopped producing a partial condition");
			require(HasDiagnostic(result,
						"condition object must have one operator") &&
					HasDiagnostic(result, "unknown condition operator") &&
					HasDiagnostic(result,
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

		runner.test("MCM mapped bindings cache their runtime key", [] {
			const auto result = ParseConfig(kSyntheticConfig, "binding-key.json");
			const auto& page = PageNamed(result, "$EXAMPLE_SOURCES");
			const auto binding = std::ranges::find_if(
				page.rows,
				[](const MappedRow& a_row) { return a_row.binding.has_value(); });
			require(binding != page.rows.end() &&
					!binding->binding->cacheKey.empty() &&
					binding->binding->cacheKey ==
						MakeBindingKey(*binding->binding),
				"a mapped binding did not retain its computed cache key");
		});

		runner.test("MCM Skyrim hiddenToggle spelling stays outside FO4 scope", [] {
			const auto result = ParseConfig(R"json({
				"modName":"HiddenToggleFixture",
				"content":[
					{"id":"bHidden:Main","type":"hiddenToggle","groupControl":1,
					 "valueOptions":{"sourceType":"ModSettingBool"}},
					{"id":"dependent","type":"text","groupCondition":1}
				]
			})json", "hidden-toggle.json");
			const auto& declared = ControlNamed(
				result.configuration->pages.front(),
				"bHidden:Main");
			require(declared.type == ControlType::kUnknown,
				"Skyrim hiddenToggle was treated as FO4 vocabulary");
			require(DescriptorCount(result.pages.front()) == 2 &&
					std::ranges::count_if(
						result.pages.front().rows,
						[](const MappedRow& a_row) {
							return a_row.binding.has_value();
						}) == 1,
				"hiddenToggle stopped emitting its current visible descriptor");
			require(HasDiagnostic(result, "unknown MCM control type",
						"$.content[0]"),
				"hiddenToggle stopped producing its current diagnostic");
		});

		runner.test("MCM images and typed actions survive mapping", [] {
			const auto result = ParseConfig(R"json({
				"modName":"MetadataFixture",
				"content":[
					{"id":"image","type":"image","libName":"Fixture","className":"Header"},
					{"id":"function","type":"button","action":{
						"type":"CallFunction","form":"Fixture.esp|800",
						"scriptName":"Fixture:Script","function":"Apply",
						"params":["{i}42","{f}1.5","{b}true","{s}text","{i}{value}"]}},
					{"id":"global","type":"button","action":{
						"type":"CallGlobalFunction","script":"FixtureGlobal",
						"function":"Apply"}},
					{"id":"external","type":"button","action":{
						"type":"CallExternalFunction","plugin":"FixtureNative",
						"function":"Apply"}},
					{"id":"console","type":"button","action":{
						"type":"RunConsoleCommand","command":"help fixture"}},
					{"id":"event","type":"button","action":{
						"type":"SendEvent","event":"FixtureEvent","params":["{value}"]}}
				]
			})json", "metadata.json");
			require(result.configuration && result.pages.size() == 1,
				"metadata fixture did not parse");
			const auto& rows = result.pages.front().rows;
			const auto image = std::ranges::find(rows, "image", &MappedRow::id);
			const auto function =
				std::ranges::find(rows, "function", &MappedRow::id);
			require(image != rows.end() && !image->emitted && image->image &&
					image->image->library == "Fixture" &&
					image->image->symbol == "Header",
				"image metadata was discarded or its row was emitted");
			require(DescriptorCount(result.pages.front()) == 0 &&
					HasDiagnostic(
						result,
						"unsupported in this phase",
						"$.content[0]"),
				"image descriptor was emitted or its warning was lost");
			const auto* call = function == rows.end() || !function->action ?
				nullptr :
				std::get_if<CallFunctionAction>(&*function->action);
			require(call && call->form == "Fixture.esp|800" &&
					call->scriptName ==
						std::optional<std::string>{ "Fixture:Script" } &&
					call->function == "Apply" && call->arguments.size() == 5,
				"CallFunction metadata was discarded");
			require(std::get<int64_t>(call->arguments[0]) == 42 &&
					std::get<double>(call->arguments[1]) == 1.5 &&
					std::get<bool>(call->arguments[2]) &&
					std::get<std::string>(call->arguments[3]) == "text",
				"typed action arguments remained encoded strings");
			const auto* substituted =
				std::get_if<ValueArgument>(&call->arguments[4]);
			require(substituted &&
					substituted->type == SourceValueKind::kInt,
				"typed value placeholder was not represented distinctly");
			require(std::holds_alternative<CallGlobalFunctionAction>(
						*std::ranges::find(rows, "global", &MappedRow::id)->action) &&
					std::holds_alternative<CallExternalFunctionAction>(
						*std::ranges::find(rows, "external", &MappedRow::id)->action) &&
					std::holds_alternative<RunConsoleCommandAction>(
						*std::ranges::find(rows, "console", &MappedRow::id)->action) &&
					std::holds_alternative<SendEventAction>(
						*std::ranges::find(rows, "event", &MappedRow::id)->action),
				"an action family was not preserved as a typed variant");
			const auto summary = SummarizeCompatibility(result.pages.front());
			require(summary.images == 1 && summary.actions == 5 &&
					summary.unsupported == 1,
				"page compatibility summary lost metadata counts");
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

		runner.test("MCM config JSON does not enable comment tolerance", [] {
			const auto result = ParseConfig(R"json({
				// Comments are accepted by the current parser.
				"modName":"CommentFixture",
				"content":[{"id":"text","type":"text"}]
			})json", "comment-config.json");
			require(!result.configuration && result.pages.empty() &&
					HasDiagnostic(result, "invalid JSON", "$"),
				"config parsing enabled unneeded JSON comment tolerance");
		});

	}
}
