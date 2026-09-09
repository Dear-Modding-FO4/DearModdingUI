#pragma once

#include <GeneralTestSuite.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

struct ID3D11Device;

namespace DearModdingUIPreview
{
	struct FixtureOptions
	{
		std::optional<std::filesystem::path> mcmConfigPath;
		std::filesystem::path userKeybindsPath;
		bool mcmInstalled{ true };
		bool gameLoaded{ true };
		bool includeNavigationComparisonFixtures{};
	};

	class FixtureRunner final
	{
	public:
		FixtureRunner();
		~FixtureRunner();

		FixtureRunner(const FixtureRunner&) = delete;
		FixtureRunner(FixtureRunner&&) = delete;
		FixtureRunner& operator=(const FixtureRunner&) = delete;
		FixtureRunner& operator=(FixtureRunner&&) = delete;

		[[nodiscard]] bool Register(
			ID3D11Device* a_device,
			std::string& a_error,
			const FixtureOptions& a_options) noexcept;
		[[nodiscard]] bool ActivatePresentationScenario(
			DmuiTests::PresentationScenario a_scenario,
			std::string& a_error) noexcept;
		[[nodiscard]] uint64_t PresentationPage(
			DmuiTests::PresentationScenario a_scenario) const noexcept;
		[[nodiscard]] bool ValidatePresentationCapture(std::string& a_error) const;
		void Stop() noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
