#pragma once

#include <DearModdingUI/MCM/Availability.h>

#include <filesystem>
#include <memory>
#include <string>

namespace DmuiTestFixtures
{
	struct McmFixtureOptions
	{
		std::filesystem::path configPath;
		std::filesystem::path dataRoot;
		std::filesystem::path userKeybindsPath;
		DearModdingUI::MCM::McmState state{ true, true };
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
