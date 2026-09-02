#include <DearModdingUI/MCM/Compatibility.h>

#include "Harness.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace vmm_tests
{
	namespace
	{
		using namespace DearModdingUI::MCM;

		[[nodiscard]] std::filesystem::path FixturePath(
			std::string_view a_name)
		{
			return std::filesystem::path{ __FILE__ }.parent_path() /
				"Fixtures" /
				"MCM" /
				a_name /
				"config.json";
		}

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
				"numeric value did not match the fixture");
		}
	}

	void run_mcm_checks(Runner& runner)
	{
		runner.test("MCM LootMan fixture preserves page and group structure", [] {
			const auto result = LoadConfig(FixturePath("lootman"));
			require(result.configuration.has_value(),
				"LootMan configuration did not parse");
			require(ErrorCount(result) == 0,
				"LootMan produced parse errors");
			require(result.configuration->modName == "LootMan",
				"LootMan modName changed");
			require(result.configuration->minimumMcmVersion ==
						std::optional<int64_t>{ 2 } &&
					result.configuration->displayName == "LootMan" &&
					result.configuration->pluginRequirements ==
						std::vector<std::string>{ "LootMan.esp" },
				"LootMan top-level metadata changed");
			require(result.pages.size() == 6,
				"LootMan page count changed");

			const std::array<size_t, 6> groups{ 1, 2, 6, 4, 3, 2 };
			const std::array<size_t, 6> settings{ 1, 15, 56, 13, 11, 7 };
			for (size_t index = 0; index < result.pages.size(); ++index)
			{
				require(result.pages[index].settings.groups.size() == groups[index],
					"LootMan group shape changed");
				require(DescriptorCount(result.pages[index]) == settings[index],
					"LootMan descriptor shape changed");
			}
			require(
				result.pages.front().settings.groups.front().label ==
					"$PAGE_MAIN_ABOUT_SECTION",
				"LootMan root section label changed");
		});

		runner.test("MCM LootMan fixture maps aliases and property metadata", [] {
			const auto result = LoadConfig(FixturePath("lootman"));
			const auto& page = PageNamed(result, "$PAGE_GENERAL_SETTINGS");

			const auto& enabled = SettingNamed(page, "EnableLootMan");
			require(std::holds_alternative<dmui::CheckboxSettingControl>(
						enabled.control),
				"switcher did not map to checkbox");
			require(enabled.label ==
					"$PAGE_GENERAL_SETTINGS_ENABLE_LOOTMAN" &&
					enabled.description ==
						"$PAGE_GENERAL_SETTINGS_ENABLE_LOOTMAN_HELP",
				"switcher text or help changed");

			const auto& logLevel = SettingNamed(page, "LogLevel");
			const auto* choice =
				std::get_if<dmui::ChoiceSettingControl>(&logLevel.control);
			require(choice && choice->options.size() == 7,
				"dropdown did not map all ordered choices");
			require(choice->options.front().value == "0" &&
					choice->options.front().label ==
						"$PAGE_GENERAL_SETTINGS_LOG_LEVEL_TRACE",
				"dropdown option order changed");

			const auto& range = SettingNamed(page, "LootingRange");
			const auto* numeric =
				std::get_if<dmui::DoubleSettingControl>(&range.control);
			require(numeric && numeric->range &&
					numeric->range->minimum &&
					numeric->range->maximum,
				"float slider range was not mapped");
			RequireNear(*numeric->range->minimum, 1.0);
			RequireNear(*numeric->range->maximum, 256.0);
			RequireNear(numeric->dragSpeed, 0.5);

			const auto& declared = DeclaredPageNamed(
				*result.configuration,
				"$PAGE_GENERAL_SETTINGS");
			const auto& source = ControlNamed(declared, "LogLevel");
			require(source.valueOptions &&
					source.valueOptions->sourceType ==
						std::optional<std::string>{ "PropertyValueInt" } &&
					source.valueOptions->sourceForm ==
						std::optional<std::string>{ "LootMan.esp|F9B" } &&
					source.valueOptions->propertyName ==
						std::optional<std::string>{ "LogLevel" },
				"property source metadata did not survive");
			require(source.groupCondition &&
					source.groupCondition->type == ConditionType::kAll &&
					source.groupCondition->operands.size() == 2,
				"compound group condition did not survive");
			require(ControlKindCount(
						result,
						dmui::SettingControlKind::kUnsupported) == 14,
				"LootMan unsupported control count changed");
		});

		runner.test("MCM Portable Junk Recycler fixture maps its full shape", [] {
			const auto result = LoadConfig(
				FixturePath("portable-junk-recycler"));
			require(result.configuration.has_value(),
				"Portable Junk Recycler configuration did not parse");
			require(ErrorCount(result) == 0,
				"Portable Junk Recycler produced parse errors: " +
					ErrorMessages(result));
			require(result.pages.size() == 5,
				"Portable Junk Recycler page count changed");

			const std::array<size_t, 5> groups{ 3, 2, 3, 48, 4 };
			const std::array<size_t, 5> settings{ 6, 10, 21, 80, 12 };
			for (size_t index = 0; index < result.pages.size(); ++index)
			{
				require(result.pages[index].settings.groups.size() == groups[index],
					"Portable Junk Recycler group shape changed");
				require(DescriptorCount(result.pages[index]) == settings[index],
					"Portable Junk Recycler descriptor shape changed");
				std::unordered_set<std::string> ids;
				for (const auto& group : result.pages[index].settings.groups)
					require(ids.insert(group.id).second,
						"duplicate group id survived mapping");
			}
			require(ControlKindCount(
						result,
						dmui::SettingControlKind::kDouble) == 82 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kSigned) == 2,
				"Portable Junk Recycler numeric kinds changed");
		});

		runner.test("MCM Portable Junk Recycler keeps ranges and value sources", [] {
			const auto result = LoadConfig(
				FixturePath("portable-junk-recycler"));
			const auto& settings = PageNamed(result, "$Settings");
			const auto& multiplier =
				SettingNamed(settings, "fMultBase:GeneralOptions");
			const auto* numeric =
				std::get_if<dmui::DoubleSettingControl>(&multiplier.control);
			require(numeric && numeric->range &&
					numeric->range->minimum &&
					numeric->range->maximum,
				"multiplier slider range was not mapped");
			RequireNear(*numeric->range->minimum, 0.0);
			RequireNear(*numeric->range->maximum, 2.0);
			RequireNear(numeric->dragSpeed, 0.01);
			require(multiplier.label == "$MultBaseText" &&
					multiplier.description == "$MultBaseHelp",
				"multiplier label or help changed");

			const auto& fractional = SettingNamed(
				settings,
				"iFractionalComponentHandling:GeneralOptions");
			const auto* choice =
				std::get_if<dmui::ChoiceSettingControl>(&fractional.control);
			require(choice && choice->options.size() == 3 &&
					choice->options[2].label == "$RoundDown",
				"fractional handling choices changed");

			const auto& advanced = PageNamed(result, "$Advanced");
			const auto& threads = SettingNamed(
				advanced,
				"iThreadLimit:Advanced");
			const auto* integer =
				std::get_if<dmui::SignedSettingControl>(&threads.control);
			require(integer && integer->range &&
					integer->range->minimum == std::optional<int64_t>{ 1 } &&
					integer->range->maximum == std::optional<int64_t>{ 32 },
				"integer slider did not retain its range");

			const auto& declared = DeclaredPageNamed(
				*result.configuration,
				"$Settings");
			const auto& source = ControlNamed(
				declared,
				"fMultBase:GeneralOptions");
			require(source.valueOptions &&
					source.valueOptions->sourceType ==
						std::optional<std::string>{ "ModSettingFloat" } &&
					source.valueOptions->modSettingId ==
						std::optional<std::string>{
							"fMultBase:GeneralOptions" },
				"ModSetting id did not survive");

			const auto& adjustments = DeclaredPageNamed(
				*result.configuration,
				"$MultiplierAdjustments");
			const auto hidden = std::ranges::find_if(
				adjustments.controls,
				[](const Control& a_control) {
					return a_control.type == ControlType::kHidden &&
						a_control.valueOptions &&
						a_control.valueOptions->propertyName ==
							std::optional<std::string>{
								"MCM_GeneralMultAdjustSimple" };
				});
			require(hidden != adjustments.controls.end() &&
					hidden->valueOptions->scriptName ==
						std::optional<std::string>{
							"PortableJunkRecyclerMk2:PJRM2_SettingManager" } &&
					hidden->valueOptions->sourceForm ==
						std::optional<std::string>{
							"Portable Junk Recycler Mk 2.esp|800" },
				"hidden property metadata did not survive");
		});

		runner.test("MCM demonstration fixture maps readable and unsupported controls", [] {
			const auto result = LoadConfig(
				FixturePath("mcm-demonstration"));
			require(result.configuration.has_value(),
				"MCM demonstration did not parse");
			require(ErrorCount(result) == 0,
				"MCM demonstration produced parse errors");
			require(result.pages.size() == 4,
				"MCM demonstration page count changed");
			const std::array<size_t, 4> settings{ 3, 2, 3, 2 };
			for (size_t index = 0; index < result.pages.size(); ++index)
				require(DescriptorCount(result.pages[index]) == settings[index],
					"MCM demonstration descriptor shape changed");

			const auto& root = PageNamed(result, "Scrivener07");
			require(dmui::ResolveSettingControlPresentation(
						root.settings.groups.front().settings.front().control).kind ==
						dmui::SettingControlKind::kReadOnly,
				"text did not map to read-only");
			require(ControlKindCount(
						result,
						dmui::SettingControlKind::kUnsupported) == 2,
				"image and button did not map to unsupported");

			const auto& ini = PageNamed(result, "INI Setting");
			const auto& opacity = SettingNamed(ini, "fHUDOpacity:INIT");
			const auto* numeric =
				std::get_if<dmui::DoubleSettingControl>(&opacity.control);
			require(numeric && numeric->range &&
					numeric->range->minimum ==
						std::optional<double>{ 0.0 } &&
					numeric->range->maximum ==
						std::optional<double>{ 1.0 },
				"opacity slider range changed");
			require(opacity.description ==
					"This slider controls the HUD opacity. Range: 0.0-1.0",
				"opacity help was not preserved");
			require(std::holds_alternative<dmui::CheckboxSettingControl>(
						SettingNamed(
							ini,
							"bCrosshairEnabled:INIT").control),
				"demonstration switcher did not map to checkbox");

			const auto& global = DeclaredPageNamed(
				*result.configuration,
				"Global Setting");
			const auto& gameHour = ControlNamed(global, "fGameHour:GLOB");
			require(gameHour.valueOptions &&
					gameHour.valueOptions->sourceType ==
						std::optional<std::string>{ "GlobalValue" } &&
					gameHour.valueOptions->sourceForm ==
						std::optional<std::string>{ "Fallout4.esm|38" },
				"global source metadata did not survive");
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
			const auto path = FixturePath("does-not-exist");
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
					declared.valueOptions->sourceType ==
						std::optional<std::string>{ "PropertyValueFloat" } &&
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
					std::get<double>(mapped.defaultValue) == 1.25,
				"numeric format or default did not map");

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
					DescriptorCount(result.pages.front()) == 10,
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
						dmui::SettingControlKind::kReadOnly) == 1 &&
					ControlKindCount(
						result,
						dmui::SettingControlKind::kUnsupported) == 4,
				"documented control kinds changed");
			require(!std::ranges::any_of(
						result.pages.front().settings.groups.front().settings,
						[](const dmui::SettingDescriptor& a_setting) {
							return a_setting.id == "hidden";
						}),
				"hidden control was emitted");
		});
	}
}
