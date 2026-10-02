#include "FixtureRunner.h"
#include "PreviewOptions.h"

namespace DearModdingUIPreview
{
	struct FixtureRunner::Impl
	{};

	FixtureRunner::FixtureRunner() = default;
	FixtureRunner::~FixtureRunner() = default;

	bool FixtureRunner::Register(
		ID3D11Device*,
		const PreviewOptions& a_options,
		std::wstring& a_error) noexcept
	{
		if (a_options.page || a_options.presentationScenario ||
			a_options.hotkeyState || a_options.syntheticHealth)
		{
			a_error = L"The requested preview scenario requires local fixtures.";
			return false;
		}
		return true;
	}

	bool FixtureRunner::ActivatePresentationScenario(std::wstring&) noexcept { return true; }
	bool FixtureRunner::PresentationUsesMenu() const noexcept { return true; }
	uint64_t FixtureRunner::PresentationPage() const noexcept { return 0; }
	bool FixtureRunner::BeforeFrame(std::wstring&) const { return true; }
	void FixtureRunner::PrepareInput(std::optional<uint32_t>) {}
	void FixtureRunner::PrepareCaptureFrame(std::optional<uint32_t>) {}
	bool FixtureRunner::BeforeDraw(std::wstring&) const { return true; }
	bool FixtureRunner::ValidateCapture(std::wstring&) const { return true; }
	void FixtureRunner::Stop() noexcept {}
}
