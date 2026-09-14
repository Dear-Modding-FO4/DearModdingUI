#pragma once

#include "../Harness.h"

#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/TextMarkup.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace vmm_tests::support::mcm
{
	using namespace DearModdingUI::MCM;

	[[nodiscard]] std::filesystem::path TemporaryConfigPath(
		std::string_view a_name);

	struct TemporaryFileCleanup
	{
		std::filesystem::path path;

		~TemporaryFileCleanup();
	};

	[[nodiscard]] const dmui::SettingDescriptor& SettingNamed(
		const MappedPage& a_page,
		std::string_view a_id);
	[[nodiscard]] const MappedRow& RowNamed(
		const MappedPage& a_page,
		std::string_view a_id);
	[[nodiscard]] const Control& ControlNamed(
		const Page& a_page,
		std::string_view a_id);
	[[nodiscard]] const GroupCondition& ConditionNamed(
		const Page& a_page,
		std::string_view a_id);
	[[nodiscard]] size_t DescriptorCount(const MappedPage& a_page);
	[[nodiscard]] bool HasDiagnostic(
		const LoadResult& a_result,
		std::string_view a_message,
		std::string_view a_location = {});
	[[nodiscard]] size_t ErrorCount(const LoadResult& a_result);
	[[nodiscard]] size_t DiagnosticCount(
		const LoadResult& a_result,
		std::string_view a_message);
	[[nodiscard]] std::string ErrorMessages(const LoadResult& a_result);
}
