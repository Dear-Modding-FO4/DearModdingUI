#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI::MCM
{
	class DiagnosticReporter;

	using FileListingResult =
		std::expected<std::vector<std::string>, std::string>;

	class FileListingAdapter
	{
	public:
		FileListingAdapter() = default;
		virtual ~FileListingAdapter() = default;

		FileListingAdapter(const FileListingAdapter&) = delete;
		FileListingAdapter(FileListingAdapter&&) = delete;
		FileListingAdapter& operator=(const FileListingAdapter&) = delete;
		FileListingAdapter& operator=(FileListingAdapter&&) = delete;

		[[nodiscard]] virtual FileListingResult List(
			std::string_view a_path,
			std::string_view a_mask) = 0;
	};

	class FileChoiceState
	{
	public:
		FileChoiceState();
		~FileChoiceState();

		FileChoiceState(const FileChoiceState&) = delete;
		FileChoiceState(FileChoiceState&&) = delete;
		FileChoiceState& operator=(const FileChoiceState&) = delete;
		FileChoiceState& operator=(FileChoiceState&&) = delete;

		[[nodiscard]] InertReason Reason() const noexcept;
		[[nodiscard]] std::string Description() const;
		[[nodiscard]] bool Update(
			FileListingResult a_listing,
			std::string_view a_path,
			std::string_view a_mask);
		void Apply(
			dmui::SettingDescriptor& a_descriptor,
			uint64_t& a_appliedGeneration) const;

	private:
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

	class FileChoiceController
	{
	public:
		FileChoiceController() = default;
		~FileChoiceController() = default;

		FileChoiceController(const FileChoiceController&) = delete;
		FileChoiceController(FileChoiceController&&) noexcept = default;
		FileChoiceController& operator=(const FileChoiceController&) = delete;
		FileChoiceController& operator=(FileChoiceController&&) noexcept = default;

		void Refresh() noexcept;
		[[nodiscard]] explicit operator bool() const noexcept;

	private:
		class Impl;
		explicit FileChoiceController(std::shared_ptr<Impl> a_impl);

		std::shared_ptr<Impl> impl_;

		friend FileChoiceController AttachFileChoices(
			MappedPage&,
			FileListingAdapter&,
			DiagnosticReporter&,
			std::string);
	};

	[[nodiscard]] FileChoiceController AttachFileChoices(
		MappedPage& a_page,
		FileListingAdapter& a_files,
		DiagnosticReporter& a_diagnostics,
		std::string a_source);
}
