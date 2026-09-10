#include "FixtureRunner.h"

#include <DearModdingUI/MCM/Win32FileListingAdapter.h>

#include "fixtures/MCMFixtures.h"
#include "fixtures/NavigationFixtures.h"

#include <d3d11.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <string_view>
#include <vector>

namespace DearModdingUIPreview
{
	namespace
	{
		class PreviewEnvironment final : public DmuiTests::Environment
		{
		public:
			void SetDevice(ID3D11Device* a_device) noexcept
			{
				m_device = a_device;
			}

			[[nodiscard]] Microsoft::WRL::ComPtr<ID3D11Device>
				AcquireRendererDevice() noexcept override
			{
				Microsoft::WRL::ComPtr<ID3D11Device> result;
				if (m_device)
					result = m_device;
				return result;
			}

			[[nodiscard]] bool SupportsGameInputContexts() const noexcept override
			{
				return false;
			}

			void Log(
				DmuiTests::LogLevel a_level,
				std::string_view a_message) noexcept override
			{
				const char* level = a_level == DmuiTests::LogLevel::kError ?
					"error" : a_level == DmuiTests::LogLevel::kWarning ?
					"warning" : "info";
				std::fprintf(
					stderr,
					"[%s] %.*s\n",
					level,
					static_cast<int>(a_message.size()),
					a_message.data());
			}

		private:
			ID3D11Device* m_device{};
		};
	}

	struct FixtureRunner::Impl
	{
		PreviewEnvironment environment;
		DmuiTests::GeneralTestSuite testSuite{ environment };
		DearModdingUI::MCM::Win32FileListingAdapter realMcmFiles;
		DmuiTestFixtures::McmFixture mcmFixture;
		std::vector<std::unique_ptr<dmui::Client>> navigationClients;
	};

	FixtureRunner::FixtureRunner() :
		m_impl(std::make_unique<Impl>())
	{}

	FixtureRunner::~FixtureRunner() = default;

	void FixtureRunner::Stop() noexcept
	{
		m_impl->testSuite.Stop();
	}

	bool FixtureRunner::Register(
		ID3D11Device* a_device,
		std::string& a_error,
		const FixtureOptions& a_options) noexcept
	{
		try
		{
			a_error.clear();
			m_impl->environment.SetDevice(a_device);
			if (!m_impl->testSuite.Initialize())
			{
				a_error = "Could not register the shared DMUI test fixture.";
				return false;
			}

			DmuiTestFixtures::McmFixtureOptions mcmOptions{
				.configPath = a_options.mcmConfigPath,
				.userKeybindsPath = a_options.userKeybindsPath,
				.state = {
					a_options.mcmInstalled,
					a_options.gameLoaded
				},
				.fileListing = a_options.mcmConfigPath ?
					static_cast<DearModdingUI::MCM::FileListingAdapter*>(
						&m_impl->realMcmFiles) :
					nullptr
			};
			if (!m_impl->mcmFixture.Register(mcmOptions, a_error))
				return false;

			if (a_options.includeNavigationComparisonFixtures &&
				!DmuiTestFixtures::RegisterNavigationComparisonFixtures(
					m_impl->navigationClients,
					a_error))
				return false;
			return true;
		}
		catch (const std::exception& a_exception)
		{
			a_error = "Preview fixture composition threw: ";
			a_error += a_exception.what();
			return false;
		}
	}

	bool FixtureRunner::ActivatePresentationScenario(
		DmuiTests::PresentationScenario a_scenario,
		std::string& a_error) noexcept
	{
		return m_impl->testSuite.ActivatePresentationScenario(
			a_scenario,
			a_error);
	}

	uint64_t FixtureRunner::PresentationPage(
		DmuiTests::PresentationScenario a_scenario) const noexcept
	{
		return m_impl->testSuite.PresentationPage(a_scenario);
	}

	bool FixtureRunner::ValidatePresentationCapture(std::string& a_error) const
	{
		return m_impl->testSuite.ValidatePresentationCapture(a_error);
	}
}
