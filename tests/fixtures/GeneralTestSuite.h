#pragma once

#include <wrl/client.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct ID3D11Device;

namespace DmuiTests
{
	enum class LogLevel
	{
		kInfo,
		kWarning,
		kError
	};

	enum class PresentationScenario
	{
		kOverlay,
		kNotification,
		kImage,
		kPlot,
		kDialog
	};

	struct PresentationScenarioDescriptor
	{
		PresentationScenario scenario;
		std::string_view name;
		std::string_view pageId;
		bool usesMenu;
	};

	inline constexpr std::array kPresentationScenarios{
		PresentationScenarioDescriptor{
			PresentationScenario::kOverlay,
			"overlay",
			"managed-overlay",
			false
		},
		PresentationScenarioDescriptor{
			PresentationScenario::kNotification,
			"notification",
			"notifications-dialogs",
			false
		},
		PresentationScenarioDescriptor{
			PresentationScenario::kImage,
			"image",
			"images",
			true
		},
		PresentationScenarioDescriptor{
			PresentationScenario::kPlot,
			"plot",
			"plot",
			true
		},
		PresentationScenarioDescriptor{
			PresentationScenario::kDialog,
			"dialog",
			"notifications-dialogs",
			true
		}
	};

	[[nodiscard]] constexpr std::optional<PresentationScenario>
		ParsePresentationScenario(std::string_view a_name) noexcept
	{
		for (const auto& descriptor : kPresentationScenarios)
		{
			if (descriptor.name == a_name)
				return descriptor.scenario;
		}
		return std::nullopt;
	}

	[[nodiscard]] constexpr bool PresentationUsesMenu(
		PresentationScenario a_scenario) noexcept
	{
		for (const auto& descriptor : kPresentationScenarios)
		{
			if (descriptor.scenario == a_scenario)
				return descriptor.usesMenu;
		}
		return false;
	}

	class PresentationScenarioState
	{
	public:
		[[nodiscard]] bool Activate(PresentationScenario a_scenario) noexcept
		{
			if (m_active)
				return false;
			m_active = a_scenario;
			return true;
		}

		[[nodiscard]] std::optional<PresentationScenario> Active() const noexcept
		{
			return m_active;
		}

	private:
		std::optional<PresentationScenario> m_active;
	};

	class Environment
	{
	public:
		virtual ~Environment() = default;

		[[nodiscard]] virtual Microsoft::WRL::ComPtr<ID3D11Device>
			AcquireRendererDevice() noexcept = 0;
		[[nodiscard]] virtual bool SupportsGameInputContexts() const noexcept = 0;
		virtual void Log(LogLevel a_level, std::string_view a_message) noexcept = 0;
	};

	class GeneralTestSuite final
	{
	public:
		explicit GeneralTestSuite(Environment& a_environment);
		~GeneralTestSuite();

		GeneralTestSuite(const GeneralTestSuite&) = delete;
		GeneralTestSuite(GeneralTestSuite&&) = delete;
		GeneralTestSuite& operator=(const GeneralTestSuite&) = delete;
		GeneralTestSuite& operator=(GeneralTestSuite&&) = delete;

		[[nodiscard]] bool Initialize() noexcept;
		[[nodiscard]] bool ActivatePresentationScenario(
			PresentationScenario a_scenario,
			std::string& a_error) noexcept;
		[[nodiscard]] uint64_t PresentationPage(
			PresentationScenario a_scenario) const noexcept;
		[[nodiscard]] bool ValidatePresentationCapture(
			std::string& a_error) const;
		void Stop() noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
