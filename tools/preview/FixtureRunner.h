#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

struct ID3D11Device;

namespace DearModdingUIPreview
{
	struct PreviewOptions;

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
			const PreviewOptions& a_options,
			std::wstring& a_error) noexcept;
		[[nodiscard]] bool ActivatePresentationScenario(
			std::wstring& a_error) noexcept;
		[[nodiscard]] bool PresentationUsesMenu() const noexcept;
		[[nodiscard]] uint64_t PresentationPage() const noexcept;
		[[nodiscard]] bool BeforeFrame(std::wstring& a_error) const;
		void PrepareInput(std::optional<uint32_t> a_frame);
		void PrepareCaptureFrame(std::optional<uint32_t> a_frame);
		[[nodiscard]] bool BeforeDraw(std::wstring& a_error) const;
		[[nodiscard]] bool ValidateCapture(std::wstring& a_error) const;
		void Stop() noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
