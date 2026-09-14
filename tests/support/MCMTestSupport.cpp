#include "MCMTestSupport.h"

#include <algorithm>
#include <chrono>
#include <system_error>

namespace vmm_tests::support::mcm
{
	using namespace DearModdingUI::MCM;

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
}
