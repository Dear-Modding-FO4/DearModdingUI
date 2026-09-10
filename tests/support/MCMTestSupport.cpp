#include "MCMTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <system_error>

namespace vmm_tests::support::mcm
{
	using namespace DearModdingUI::MCM;

const std::string_view kSyntheticConfig = R"json({
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

TemporaryFileCleanup::~TemporaryFileCleanup()
{
	std::error_code error;
	std::filesystem::remove(path, error);
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
	std::string_view a_location)
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
