#pragma once

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/navigation/SidebarComparison.h>

#include <GeneralTestSuite.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace DearModdingUIPreview
{
	struct PreviewOptions
	{
		uint32_t width{ 3840 };
		uint32_t height{ 2160 };
		uint32_t frames{ 3 };
		std::optional<std::filesystem::path> screenshot;
		std::optional<std::string> page;
		std::optional<DearModdingUI::HostPageKind> hostPage;
		std::optional<std::vector<std::string>> expandedMods;
		std::optional<DearModdingUI::SidebarLayoutKind> sidebarOverride;
		std::optional<DearModdingUI::NavigationPresentationKind>
			navigationOverride;
		std::optional<DMUI_ClientOrigin> navigationOrigin;
		std::optional<DmuiTests::PresentationScenario> presentationScenario;
		bool syntheticHealth{};
		bool help{};
	};

	[[nodiscard]] bool ParsePreviewOptions(
		int a_argumentCount,
		wchar_t** a_arguments,
		PreviewOptions& a_options,
		std::wstring& a_error);
	void PrintPreviewUsage();
}
