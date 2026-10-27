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
	class TaskScheduler;

	using FileListingResult =
		std::expected<std::vector<std::string>, std::string>;

	// The failure may be bridge text, so it renders in the viewer's language.
	using FileScanResult =
		std::expected<std::vector<std::string>, Support::Message>;

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
		explicit FileChoiceState(
			const Support::TextCatalog& a_catalog = Support::EnglishCatalog());
		~FileChoiceState();

		FileChoiceState(const FileChoiceState&) = delete;
		FileChoiceState(FileChoiceState&&) = delete;
		FileChoiceState& operator=(const FileChoiceState&) = delete;
		FileChoiceState& operator=(FileChoiceState&&) = delete;

		[[nodiscard]] InertReason Reason() const noexcept;
		[[nodiscard]] Support::Message Description() const;
		[[nodiscard]] bool Update(
			FileScanResult a_listing,
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
			TaskScheduler&,
			DiagnosticReporter&,
			std::string,
			const Support::TextCatalog&);
	};

	// Refresh lists files through the scheduler's background executor.
	[[nodiscard]] FileChoiceController AttachFileChoices(
		MappedPage& a_page,
		FileListingAdapter& a_files,
		TaskScheduler& a_scheduler,
		DiagnosticReporter& a_diagnostics,
		std::string a_source,
		const Support::TextCatalog& a_catalog = Support::EnglishCatalog());
}
