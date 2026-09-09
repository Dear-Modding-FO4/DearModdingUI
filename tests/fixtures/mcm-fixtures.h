#pragma once

#include <DearModdingUI/MCM/Availability.h>
#include <DearModdingUI/MCM/FileChoices.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace DmuiTestFixtures
{
	class BuiltinMcmFileListingAdapter final :
		public DearModdingUI::MCM::FileListingAdapter
	{
	public:
		[[nodiscard]] DearModdingUI::MCM::FileListingResult List(
			std::string_view a_path,
			std::string_view a_mask) override;
	};

	struct McmFixtureOptions
	{
		std::optional<std::filesystem::path> configPath;
		std::filesystem::path userKeybindsPath;
		DearModdingUI::MCM::McmState state{ true, true };
		DearModdingUI::MCM::FileListingAdapter* fileListing{};
	};

	class McmFixture final
	{
	public:
		McmFixture();
		~McmFixture();

		McmFixture(const McmFixture&) = delete;
		McmFixture(McmFixture&&) = delete;
		McmFixture& operator=(const McmFixture&) = delete;
		McmFixture& operator=(McmFixture&&) = delete;

		[[nodiscard]] bool Register(
			const McmFixtureOptions& a_options,
			std::string& a_error) noexcept;
		[[nodiscard]] size_t PageCount() const noexcept;
		[[nodiscard]] size_t DiagnosticCount() const noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
