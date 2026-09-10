#include "PreviewApplication.h"

#include "FixtureRunner.h"
#include "PreviewRenderer.h"
#include "PreviewWindow.h"
#include "fixtures/HostHealthFixtures.h"

#include <DearModdingUI/presentation/BackgroundBlur.h>
#include <Platform/input/CursorLoader.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/host/Shell.h>
#include <DearModdingUI/navigation/SidebarComparison.h>
#include <DearModdingUI/presentation/Theme.h>
#include <Support/Runtime.h>
#include <Support/SubsystemHealth.h>

#include <Windows.h>

#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace DearModdingUIPreview
{
	using namespace DearModdingUI;

	namespace
	{
		class PreviewHealthReporter final : public HealthReporter
		{
		public:
			void Report(
				HealthEvent,
				const HealthSnapshot&) noexcept override
			{}
		};

		PreviewHealthReporter g_previewHealthReporter;
	}

	struct PreviewApplication::Impl
	{
		explicit Impl(PreviewOptions a_options) :
			options(std::move(a_options)),
			window(renderer)
		{}

		~Impl()
		{
			if (fixtures)
				fixtures->Stop();
			PresentationServices::InvalidateDevice();
			window.SetImguiBackendReady(false);
			if (dx11Initialized)
				ImGui_ImplDX11_Shutdown();
			if (win32Initialized)
				ImGui_ImplWin32_Shutdown();
			if (context)
			{
				CursorLoader::Shutdown();
				BackgroundBlur::ResetDeviceResources();
				fixtures.reset();
				ImGui::DestroyContext(context);
			}
		}

		[[nodiscard]] int Run()
		{
			std::wstring error;
			if (!Initialize(error))
			{
				std::wcerr << L"dmui-preview: " << error << L'\n';
				return 1;
			}
			if (options.screenshot)
				return RunCapture(error);
			return RunInteractive(error);
		}

		[[nodiscard]] bool Initialize(std::wstring& a_error)
		{
			ImGui_ImplWin32_EnableDpiAwareness();
			if (!window.Create(
					options.width,
					options.height,
					options.screenshot.has_value(),
					a_error) ||
				!renderer.Initialize(
					window.Handle(),
					options.width,
					options.height,
					a_error))
				return false;

			IMGUI_CHECKVERSION();
			context = ImGui::CreateContext();
			if (!context)
			{
				a_error = L"ImGui::CreateContext failed.";
				return false;
			}
			auto& io = ImGui::GetIO();
			io.ConfigFlags |=
				ImGuiConfigFlags_NavEnableKeyboard |
				ImGuiConfigFlags_DockingEnable;
			io.IniFilename = nullptr;
			io.MouseDrawCursor = false;
			if (!options.screenshot && !ConfigureIni(io, a_error))
				return false;

			HostSettings::Initialize();
			DearModdingUI::Initialize();
			fixtures = std::make_unique<FixtureRunner>();
			std::string registrationError;
			const auto* configOverride =
				std::getenv("DMUI_PREVIEW_MCM_CONFIG");
			const auto environmentFlag = [](const char* a_name, bool a_default) {
				const auto* value = std::getenv(a_name);
				return value ? std::string_view{ value } != "0" : a_default;
			};
			const FixtureOptions fixtureOptions{
				.mcmConfigPath = configOverride ?
					std::optional{
						std::filesystem::path{ configOverride }
					} :
					std::nullopt,
				.userKeybindsPath =
					std::filesystem::current_path() /
					"Data" / "MCM" / "Settings" / "Keybinds.json",
				.mcmInstalled = environmentFlag(
					"DMUI_PREVIEW_MCM_INSTALLED",
					true),
				.gameLoaded = environmentFlag(
					"DMUI_PREVIEW_GAME_LOADED",
					true),
				.includeNavigationComparisonFixtures =
					options.navigationOverride.has_value()
			};
			if (!fixtures->Register(
					renderer.Device(),
					registrationError,
					fixtureOptions))
			{
				a_error.assign(
					registrationError.begin(),
					registrationError.end());
				return false;
			}
			Theme::Initialize(window.Handle());
			if (options.syntheticHealth)
				syntheticHealth = DmuiTestFixtures::CreateSyntheticHealth(
					HostSubsystemHealthRegistry(),
					g_previewHealthReporter);
			CursorLoader::Initialize(window.Handle());
			if (!ImGui_ImplWin32_Init(window.Handle()))
			{
				a_error = L"ImGui_ImplWin32_Init failed.";
				return false;
			}
			win32Initialized = true;
			if (!ImGui_ImplDX11_Init(renderer.Device(), renderer.Context()))
			{
				a_error = L"ImGui_ImplDX11_Init failed.";
				return false;
			}
			dx11Initialized = true;
			if (!ImGui_ImplDX11_CreateDeviceObjects())
			{
				a_error = L"ImGui_ImplDX11_CreateDeviceObjects failed.";
				return false;
			}
			window.SetImguiBackendReady(true);
			{
				RenderExecution::Guard execution{
					RenderExecution::Phase::kBackendInitialization
				};
				(void)execution.NoteBinding(1);
				PresentationServices::SetDevice(renderer.Device());

				if (!BeginBackendInitialization())
				{
					a_error = L"The host refused backend initialization.";
					return false;
				}
				CompleteBackendInitialization(context);
				if (options.presentationScenario)
				{
					std::string presentationError;
					if (!fixtures->ActivatePresentationScenario(
							*options.presentationScenario,
							presentationError))
					{
						a_error.assign(
							presentationError.begin(),
							presentationError.end());
						return false;
					}
				}
			}
			if (!SelectInitialPage(a_error))
				return false;
			return ConfigureSidebar(a_error);
		}

		[[nodiscard]] bool ConfigureSidebar(std::wstring& a_error)
		{
			if (options.expandedMods)
			{
				const auto& clients = Navigation().clients;
				for (const auto& id : *options.expandedMods)
				{
					if (std::ranges::find(clients, id, &NavigationClient::id) ==
						clients.end())
					{
						a_error = L"Expanded mod ID was not registered.";
						return false;
					}
				}
			}
			ConfigurePreviewSidebarComparison(
				options.sidebarOverride,
				options.expandedMods.has_value(),
				options.expandedMods ?
					std::span<const std::string>{ *options.expandedMods } :
					std::span<const std::string>{},
				options.navigationOverride,
				options.navigationOrigin.value_or(
					DMUI_CLIENT_ORIGIN_NATIVE));
			return true;
		}

		[[nodiscard]] bool ConfigureIni(
			ImGuiIO& a_io,
			std::wstring& a_error)
		{
			std::filesystem::path directory{
				Addictol::Support::GetRuntimeDirectory()
			};
			directory /= L"Data\\F4SE\\Plugins\\DearModdingUI";
			std::error_code error;
			std::filesystem::create_directories(directory, error);
			if (error)
			{
				a_error = L"Could not create the ImGui settings directory.";
				return false;
			}
			iniPath = (directory / L"imgui.ini").string();
			a_io.IniFilename = iniPath.c_str();
			a_io.IniSavingRate = 10.0f;
			return true;
		}

		[[nodiscard]] bool SelectInitialPage(std::wstring& a_error)
		{
			if (options.presentationScenario)
			{
				const auto scenario = *options.presentationScenario;
				if (!DmuiTests::PresentationUsesMenu(scenario))
				{
					(void)SetMenuVisible(false);
					return true;
				}
				const auto scenarioPage = fixtures->PresentationPage(scenario);
				const auto& pages = OrderedPages();
				const auto page = std::ranges::find(
					pages,
					scenarioPage,
					&RegisteredPage::handle);
				if (SetMenuVisible(true) != DMUI_RESULT_OK ||
					page == pages.end() ||
					HostAPI().selectPage(
						page->client,
						scenarioPage) != DMUI_RESULT_OK)
				{
					a_error =
						L"Could not open the presentation service page.";
					return false;
				}
				return true;
			}
			if (options.page && options.hostPage)
			{
				a_error = L"Choose either --page or --host-page.";
				return false;
			}
			if (!options.page)
			{
				if (SetMenuVisible(true) == DMUI_RESULT_OK)
				{
					if (options.hostPage)
						ConfigurePreviewHostPage(*options.hostPage);
					else if (options.navigationOrigin)
					{
						const auto presentation =
							BuildNavigationPresentation(
								NavigationPresentationKind::Destinations,
								Navigation(),
								{
									.destinations = { *options.navigationOrigin }
								});
						if (presentation.sections.empty())
						{
							a_error =
								L"No settings client matched the requested origin.";
							return false;
						}
						const auto& section = Navigation().sections[
							presentation.sections.front().sectionIndex];
						if (section.clientIndices.empty())
						{
							a_error =
								L"No settings client matched the requested origin.";
							return false;
						}
						const auto* client = &Navigation().clients[
							section.clientIndices.front()];
						const auto page = ResolveLandingPage(*client);
						if (HostAPI().selectPage(
								client->handle,
								page) != DMUI_RESULT_OK)
						{
							a_error =
								L"Could not select the requested origin's initial page.";
							return false;
						}
					}
					return true;
				}
				a_error = L"Could not open the host menu.";
				return false;
			}

			const auto separator = options.page->find('/');
			const std::string_view clientId{
				options.page->data(),
				separator
			};
			const std::string_view pageId{
				options.page->data() + separator + 1,
				options.page->size() - separator - 1
			};
			const auto& pages = OrderedPages();
			const auto page = std::ranges::find_if(
				pages,
				[&](const RegisteredPage& a_page) {
					return a_page.clientId == clientId &&
						a_page.id == pageId;
				});
			if (page == pages.end())
			{
				a_error = L"Requested page was not registered.";
				return false;
			}
			if (options.navigationOrigin)
			{
				const auto* client = Navigation().FindClient(page->client);
				if (!client || client->origin != *options.navigationOrigin)
				{
					a_error = L"Requested page does not match --origin.";
					return false;
				}
			}
			const auto result = HostAPI().selectPage(
				page->client,
				page->handle);
			if (result != DMUI_RESULT_OK)
			{
				a_error = L"Could not select the requested page.";
				return false;
			}
			return true;
		}

		[[nodiscard]] bool RenderFrame(std::wstring& a_error)
		{
			{
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameDraw
				};
				(void)execution.NoteBinding(1);
				if (!renderer.ApplyResize(a_error) ||
					!Theme::PrepareFrame(renderer.Height()))
				{
					if (a_error.empty())
						a_error = L"Theme::PrepareFrame failed.";
					return false;
				}

				CursorLoader::PrepareFrame(IsMenuVisible());
				BackgroundBlur::BeginFrame();
				PresentationServices::BeginFrame();
				ImGui_ImplDX11_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();
				DrawDemandedOverlays();
				PresentationServices::DrawNotification();
				if (IsMenuVisible())
				{
					DrawShell();
					PresentationServices::DrawDialog(true);
					ApplyMenuEscapeDismissal();
				}
				ImGui::Render();

				renderer.Clear();
				BackgroundBlur::Render(
					renderer.Device(),
					renderer.Context(),
					renderer.BackBuffer(),
					renderer.BackBufferView());
				renderer.BindBackBuffer();
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
				PresentationServices::CompleteRenderSubmission();
			}
			{
				RenderExecution::Guard execution{
					RenderExecution::Phase::kFrameObservation
				};
				(void)execution.NoteBinding(1);
				ObserveFrame();
			}
			return true;
		}

		[[nodiscard]] int RunCapture(std::wstring& a_error)
		{
			for (uint32_t frame = 0; frame < options.frames; ++frame)
			{
				if (!window.PumpMessages())
				{
					a_error = L"Preview window closed before capture.";
					break;
				}
				if (!RenderFrame(a_error))
					break;
			}
			if (a_error.empty() && options.presentationScenario)
			{
				std::string fixtureError;
				if (!fixtures->ValidatePresentationCapture(fixtureError))
					a_error.assign(fixtureError.begin(), fixtureError.end());
			}
			if (a_error.empty())
				(void)renderer.Capture(*options.screenshot, a_error);
			if (!a_error.empty())
			{
				std::wcerr << L"dmui-preview: " << a_error << L'\n';
				return 1;
			}
			std::wcout << L"Wrote " << options.screenshot->wstring() << L'\n';
			return 0;
		}

		[[nodiscard]] int RunInteractive(std::wstring& a_error)
		{
			window.Show();
			while (window.PumpMessages())
			{
				if (IsIconic(window.Handle()))
				{
					std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
					continue;
				}
				if (!RenderFrame(a_error) || !renderer.Present(a_error))
					break;
			}
			if (!a_error.empty())
			{
				std::wcerr << L"dmui-preview: " << a_error << L'\n';
				return 1;
			}
			return 0;
		}

		PreviewOptions options;
		PreviewRenderer renderer;
		PreviewWindow window;
		ImGuiContext* context{};
		std::unique_ptr<FixtureRunner> fixtures;
		std::vector<std::unique_ptr<SubsystemHealth>> syntheticHealth;
		std::string iniPath;
		bool win32Initialized{};
		bool dx11Initialized{};
	};

	PreviewApplication::PreviewApplication(PreviewOptions a_options) :
		m_impl(std::make_unique<Impl>(std::move(a_options)))
	{}

	PreviewApplication::~PreviewApplication() = default;

	int PreviewApplication::Run()
	{
		return m_impl->Run();
	}
}
