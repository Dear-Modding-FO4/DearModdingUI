#include "PreviewOptions.h"

#include <DearModdingUI/navigation/SidebarComparison.h>

#include <Windows.h>

#include <iostream>
#include <optional>
#include <string_view>

namespace DearModdingUIPreview
{
	using namespace DearModdingUI;

	namespace
	{
		inline constexpr uint32_t kMaximumDimension{ 16384 };
		inline constexpr uint32_t kMaximumFrames{ 10000 };

		[[nodiscard]] bool ParseUnsigned(
			std::wstring_view a_text,
			uint32_t a_maximum,
			uint32_t& a_value) noexcept
		{
			if (a_text.empty())
				return false;
			uint64_t result{};
			for (const auto character : a_text)
			{
				if (character < L'0' || character > L'9')
					return false;
				result = result * 10u +
					static_cast<uint64_t>(character - L'0');
				if (result > a_maximum)
					return false;
			}
			if (result == 0)
				return false;
			a_value = static_cast<uint32_t>(result);
			return true;
		}

		[[nodiscard]] std::optional<std::string> WideToUtf8(
			std::wstring_view a_text)
		{
			if (a_text.empty())
				return std::string{};
			const auto length = WideCharToMultiByte(
				CP_UTF8,
				WC_ERR_INVALID_CHARS,
				a_text.data(),
				static_cast<int>(a_text.size()),
				nullptr,
				0,
				nullptr,
				nullptr);
			if (length <= 0)
				return std::nullopt;
			std::string result(static_cast<size_t>(length), '\0');
			if (WideCharToMultiByte(
					CP_UTF8,
					WC_ERR_INVALID_CHARS,
					a_text.data(),
					static_cast<int>(a_text.size()),
					result.data(),
					length,
					nullptr,
					nullptr) != length)
				return std::nullopt;
			return result;
		}
	}

	void PrintPreviewUsage()
	{
		std::wcout
			<< L"Usage: dmui-preview [options]\n"
			<< L"  --screenshot <path.png>  Capture a PNG and exit\n"
			<< L"  --frames <n>             Frames before capture (default 3)\n"
			<< L"  --width <n>              Backbuffer width (default 3840)\n"
			<< L"  --height <n>             Backbuffer height (default 2160)\n"
			<< L"  --page <client-id/page-id>  Open a registered settings page\n"
			<< L"  --host-page <home|health|settings>  Open a host page\n"
			<< L"  --sidebar <tree|twopane|drilldown|iconrail>  Select the sidebar layout\n"
			<< L"  --navigation <grouped|destinations>  Enable a preview-only navigation comparison\n"
			<< L"  --origin <native|bridged>  Select the destinations comparison tab\n"
			<< L"  --presentation <overlay|notification|image|plot|dialog>\n"
			<< L"                            Activate a shared exercise capture state\n"
			<< L"  --health-scenario <synthetic>  Add labeled synthetic Health states\n"
			<< L"  --expand <client-id>      Expand a tree mod or enter a drill-down mod\n"
			<< L"  --collapse-all            Collapse the tree or show the drill-down root\n"
			<< L"  --help                    Show this help\n";
	}

	bool ParsePreviewOptions(
		int a_argumentCount,
		wchar_t** a_arguments,
		PreviewOptions& a_options,
		std::wstring& a_error)
	{
		for (int index = 1; index < a_argumentCount; ++index)
		{
			const std::wstring_view argument{ a_arguments[index] };
			if (argument == L"--help")
			{
				a_options.help = true;
				continue;
			}
			if (argument == L"--collapse-all")
			{
				a_options.expandedMods.emplace();
				continue;
			}
			if (index + 1 >= a_argumentCount)
			{
				a_error = L"Missing value for " + std::wstring{ argument } + L".";
				return false;
			}
			const std::wstring_view value{ a_arguments[++index] };
			if (argument == L"--screenshot")
			{
				if (value.empty())
				{
					a_error = L"Screenshot path cannot be empty.";
					return false;
				}
				a_options.screenshot = std::filesystem::path{ value };
			}
			else if (argument == L"--frames")
			{
				if (!ParseUnsigned(value, kMaximumFrames, a_options.frames))
				{
					a_error = L"Frame count must be between 1 and 10000.";
					return false;
				}
			}
			else if (argument == L"--width")
			{
				if (!ParseUnsigned(value, kMaximumDimension, a_options.width))
				{
					a_error = L"Width must be between 1 and 16384.";
					return false;
				}
			}
			else if (argument == L"--height")
			{
				if (!ParseUnsigned(value, kMaximumDimension, a_options.height))
				{
					a_error = L"Height must be between 1 and 16384.";
					return false;
				}
			}
			else if (argument == L"--page")
			{
				const auto page = WideToUtf8(value);
				if (!page)
				{
					a_error = L"Page selector is not valid UTF-8.";
					return false;
				}
				const auto separator = page->find('/');
				if (separator == std::string::npos ||
					separator == 0 ||
					separator + 1 == page->size())
				{
					a_error = L"Page selector must be <client-id>/<page-id>.";
					return false;
				}
				a_options.page = *page;
			}
			else if (argument == L"--host-page")
			{
				const auto page = WideToUtf8(value);
				if (!page)
				{
					a_error = L"Host page selector is not valid UTF-8.";
					return false;
				}
				if (*page == "home")
					a_options.hostPage = HostPageKind::kHome;
				else if (*page == "health")
					a_options.hostPage = HostPageKind::kHealth;
				else if (*page == "settings")
					a_options.hostPage = HostPageKind::kSettings;
				else
				{
					a_error = L"Host page must be home, health, or settings.";
					return false;
				}
			}
			else if (argument == L"--sidebar")
			{
				const auto name = WideToUtf8(value);
				const auto layout = name ?
					ParseSidebarLayout(*name) :
					std::nullopt;
				if (!layout)
				{
					a_error =
						L"Sidebar layout must be tree, twopane, drilldown, or iconrail.";
					return false;
				}
				a_options.sidebarOverride = *layout;
			}
			else if (argument == L"--navigation")
			{
				const auto name = WideToUtf8(value);
				const auto navigation = name ?
					ParsePreviewNavigationKind(*name) :
					std::nullopt;
				if (!navigation)
				{
					a_error =
						L"Navigation comparison must be grouped or destinations.";
					return false;
				}
				if (a_options.navigationOverride)
				{
					a_error = L"Navigation comparison was specified more than once.";
					return false;
				}
				a_options.navigationOverride = *navigation;
			}
			else if (argument == L"--origin")
			{
				const auto name = WideToUtf8(value);
				const auto origin = name ?
					ParsePreviewNavigationOrigin(*name) :
					std::nullopt;
				if (!origin)
				{
					a_error = L"Navigation origin must be native or bridged.";
					return false;
				}
				if (a_options.navigationOrigin)
				{
					a_error = L"Navigation origin was specified more than once.";
					return false;
				}
				a_options.navigationOrigin = *origin;
			}
			else if (argument == L"--presentation")
			{
				const auto name = WideToUtf8(value);
				const auto scenario = name ?
					DmuiTests::ParsePresentationScenario(*name) :
					std::nullopt;
				if (!scenario)
				{
					a_error =
						L"Presentation must be overlay, notification, image, plot, or dialog.";
					return false;
				}
				a_options.presentationScenario = *scenario;
			}
			else if (argument == L"--health-scenario")
			{
				if (value != L"synthetic")
				{
					a_error = L"Health scenario must be synthetic.";
					return false;
				}
				a_options.syntheticHealth = true;
			}
			else if (argument == L"--expand")
			{
				const auto client = WideToUtf8(value);
				if (!client || client->empty())
				{
					a_error = L"Expanded mod ID is not valid UTF-8.";
					return false;
				}
				if (!a_options.expandedMods)
					a_options.expandedMods.emplace();
				a_options.expandedMods->push_back(*client);
			}
			else
			{
				a_error = L"Unknown option " + std::wstring{ argument } + L".";
				return false;
			}
		}
		if (a_options.navigationOrigin &&
			a_options.navigationOverride !=
				NavigationPresentationKind::Destinations)
		{
			a_error = L"--origin requires --navigation destinations.";
			return false;
		}
		if (a_options.navigationOverride ==
				NavigationPresentationKind::Destinations &&
			!a_options.navigationOrigin)
		{
			a_error =
				L"--navigation destinations requires --origin native or bridged.";
			return false;
		}
		return true;
	}
}
