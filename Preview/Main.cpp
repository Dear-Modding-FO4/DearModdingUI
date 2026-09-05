#include "FakeData.h"

#include <DearModdingUI/BackgroundBlur.h>
#include <DearModdingUI/CursorLoader.h>
#include <DearModdingUI/Host.h>
#include <DearModdingUI/HostSettings.h>
#include <DearModdingUI/Shell.h>
#include <DearModdingUI/Theme.h>
#include <DearModdingUI/SidebarComparison.h>
#include <Support/Runtime.h>

#include <Windows.h>
#include <d3d11.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND a_window,
	UINT a_message,
	WPARAM a_wparam,
	LPARAM a_lparam);

namespace DearModdingUIPreview
{
	using Microsoft::WRL::ComPtr;
	using namespace DearModdingUI;

	namespace
	{
		inline constexpr uint32_t kDefaultWidth{ 3840 };
		inline constexpr uint32_t kDefaultHeight{ 2160 };
		inline constexpr uint32_t kDefaultFrames{ 3 };
		inline constexpr uint32_t kMaximumDimension{ 16384 };
		inline constexpr uint32_t kMaximumFrames{ 10000 };
		inline constexpr wchar_t kWindowClassName[]{
			L"DearModdingUIPreviewWindow"
		};

		struct Options
		{
			uint32_t width{ kDefaultWidth };
			uint32_t height{ kDefaultHeight };
			uint32_t frames{ kDefaultFrames };
			std::optional<std::filesystem::path> screenshot;
			std::optional<std::string> page;
			std::optional<HostPageKind> hostPage;
			std::optional<std::vector<std::string>> expandedMods;
			std::optional<SidebarLayoutKind> sidebarOverride;
			bool help{};
		};

		class Renderer;

		Renderer* g_renderer{};
		bool g_imguiBackendReady{};

		void SetHRESULTError(
			std::wstring& a_error,
			std::wstring_view a_operation,
			HRESULT a_result)
		{
			std::wostringstream output;
			output << a_operation << L" failed (0x" << std::hex
				   << static_cast<uint32_t>(a_result) << L").";
			a_error = output.str();
		}

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

		void PrintUsage()
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
				<< L"  --expand <client-id>      Expand a tree mod or enter a drill-down mod\n"
				<< L"  --collapse-all            Collapse the tree or show the drill-down root\n"
				<< L"  --help                    Show this help\n";
		}

		[[nodiscard]] bool ParseOptions(
			int a_argumentCount,
			wchar_t** a_arguments,
			Options& a_options,
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
			return true;
		}

		class Renderer final
		{
		public:
			[[nodiscard]] bool Initialize(
				HWND a_window,
				uint32_t a_width,
				uint32_t a_height,
				std::wstring& a_error)
			{
				DXGI_SWAP_CHAIN_DESC swapChainDescription{};
				swapChainDescription.BufferCount = 2;
				swapChainDescription.BufferDesc.Width = a_width;
				swapChainDescription.BufferDesc.Height = a_height;
				swapChainDescription.BufferDesc.Format =
					DXGI_FORMAT_R8G8B8A8_UNORM;
				swapChainDescription.BufferUsage =
					DXGI_USAGE_RENDER_TARGET_OUTPUT;
				swapChainDescription.OutputWindow = a_window;
				swapChainDescription.SampleDesc.Count = 1;
				swapChainDescription.Windowed = TRUE;
				swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

				auto result = CreateDevice(
					D3D_DRIVER_TYPE_HARDWARE,
					swapChainDescription);
				if (FAILED(result))
				{
					Reset();
					result = CreateDevice(
						D3D_DRIVER_TYPE_WARP,
						swapChainDescription);
				}
				if (FAILED(result))
				{
					SetHRESULTError(
						a_error,
						L"D3D11CreateDeviceAndSwapChain",
						result);
					return false;
				}
				return CreateBackBuffer(a_error);
			}

			void RequestResize(uint32_t a_width, uint32_t a_height) noexcept
			{
				m_pendingWidth = a_width;
				m_pendingHeight = a_height;
			}

			[[nodiscard]] bool ApplyResize(std::wstring& a_error)
			{
				if (!m_pendingWidth || !m_pendingHeight)
					return true;
				const auto width = std::exchange(m_pendingWidth, 0u);
				const auto height = std::exchange(m_pendingHeight, 0u);
				if (width == m_width && height == m_height)
					return true;

				m_context->OMSetRenderTargets(0, nullptr, nullptr);
				BackgroundBlur::InvalidateBackBuffer();
				m_backBufferView.Reset();
				m_backBuffer.Reset();
				const auto result = m_swapChain->ResizeBuffers(
					0,
					width,
					height,
					DXGI_FORMAT_UNKNOWN,
					0);
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"IDXGISwapChain::ResizeBuffers", result);
					return false;
				}
				return CreateBackBuffer(a_error);
			}

			void Clear() noexcept
			{
				constexpr float color[]{ 0.025f, 0.031f, 0.043f, 1.0f };
				m_context->ClearRenderTargetView(m_backBufferView.Get(), color);
			}

			void BindBackBuffer() noexcept
			{
				auto* const view = m_backBufferView.Get();
				m_context->OMSetRenderTargets(1, &view, nullptr);
			}

			[[nodiscard]] bool Present(std::wstring& a_error)
			{
				const auto result = m_swapChain->Present(1, 0);
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"IDXGISwapChain::Present", result);
					return false;
				}
				return true;
			}

			[[nodiscard]] bool Capture(
				const std::filesystem::path& a_path,
				std::wstring& a_error)
			{
				std::error_code filesystemError;
				if (!a_path.parent_path().empty())
				{
					std::filesystem::create_directories(
						a_path.parent_path(),
						filesystemError);
					if (filesystemError)
					{
						a_error = L"Could not create the screenshot directory.";
						return false;
					}
				}

				D3D11_TEXTURE2D_DESC description{};
				m_backBuffer->GetDesc(&description);
				D3D11_TEXTURE2D_DESC stagingDescription = description;
				stagingDescription.BindFlags = 0;
				stagingDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				stagingDescription.MiscFlags = 0;
				stagingDescription.Usage = D3D11_USAGE_STAGING;

				ComPtr<ID3D11Texture2D> staging;
				auto result = m_device->CreateTexture2D(
					&stagingDescription,
					nullptr,
					staging.GetAddressOf());
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"ID3D11Device::CreateTexture2D", result);
					return false;
				}
				m_context->CopyResource(staging.Get(), m_backBuffer.Get());

				ComPtr<IWICImagingFactory> factory;
				result = CoCreateInstance(
					CLSID_WICImagingFactory,
					nullptr,
					CLSCTX_INPROC_SERVER,
					IID_PPV_ARGS(factory.GetAddressOf()));
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"CoCreateInstance(WIC)", result);
					return false;
				}

				ComPtr<IWICStream> stream;
				result = factory->CreateStream(stream.GetAddressOf());
				if (SUCCEEDED(result))
					result = stream->InitializeFromFilename(
						a_path.c_str(),
						GENERIC_WRITE);
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"IWICStream::InitializeFromFilename", result);
					return false;
				}

				ComPtr<IWICBitmapEncoder> encoder;
				result = factory->CreateEncoder(
					GUID_ContainerFormatPng,
					nullptr,
					encoder.GetAddressOf());
				if (SUCCEEDED(result))
					result = encoder->Initialize(
						stream.Get(),
						WICBitmapEncoderNoCache);
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"IWICBitmapEncoder::Initialize", result);
					return false;
				}

				ComPtr<IWICBitmapFrameEncode> frame;
				ComPtr<IPropertyBag2> properties;
				result = encoder->CreateNewFrame(
					frame.GetAddressOf(),
					properties.GetAddressOf());
				if (SUCCEEDED(result))
					result = frame->Initialize(properties.Get());
				if (SUCCEEDED(result))
					result = frame->SetSize(description.Width, description.Height);
				WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppRGBA;
				if (SUCCEEDED(result))
					result = frame->SetPixelFormat(&pixelFormat);
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"IWICBitmapFrameEncode::Initialize", result);
					return false;
				}
				const auto directRgba =
					InlineIsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppRGBA);
				const auto convertToBgra =
					InlineIsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppBGRA);
				if (!directRgba && !convertToBgra)
				{
					a_error = L"WIC does not support a 32-bit PNG pixel format.";
					return false;
				}
				const auto outputStride = description.Width * 4u;
				const auto outputSize =
					static_cast<uint64_t>(outputStride) * description.Height;
				if (convertToBgra &&
					outputSize > (std::numeric_limits<UINT>::max)())
				{
					a_error = L"Converted screenshot buffer is too large for WIC.";
					return false;
				}
				std::vector<BYTE> pixels;
				if (convertToBgra)
					pixels.resize(static_cast<size_t>(outputSize));

				D3D11_MAPPED_SUBRESOURCE mapped{};
				result = m_context->Map(
					staging.Get(),
					0,
					D3D11_MAP_READ,
					0,
					&mapped);
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"ID3D11DeviceContext::Map", result);
					return false;
				}

				const auto bufferSize =
					static_cast<uint64_t>(mapped.RowPitch) * description.Height;
				if (bufferSize > (std::numeric_limits<UINT>::max)())
				{
					m_context->Unmap(staging.Get(), 0);
					a_error = L"Screenshot buffer is too large for WIC.";
					return false;
				}
				if (directRgba)
				{
					result = frame->WritePixels(
						description.Height,
						mapped.RowPitch,
						static_cast<UINT>(bufferSize),
						static_cast<BYTE*>(mapped.pData));
				}
				else
				{
					for (uint32_t y = 0; y < description.Height; ++y)
					{
						const auto* source =
							static_cast<const BYTE*>(mapped.pData) +
							static_cast<size_t>(mapped.RowPitch) * y;
						auto* destination =
							pixels.data() + static_cast<size_t>(outputStride) * y;
						for (uint32_t x = 0; x < description.Width; ++x)
						{
							destination[x * 4u] = source[x * 4u + 2u];
							destination[x * 4u + 1u] = source[x * 4u + 1u];
							destination[x * 4u + 2u] = source[x * 4u];
							destination[x * 4u + 3u] = source[x * 4u + 3u];
						}
					}
					result = frame->WritePixels(
						description.Height,
						outputStride,
						static_cast<UINT>(outputSize),
						pixels.data());
				}
				m_context->Unmap(staging.Get(), 0);
				if (SUCCEEDED(result))
					result = frame->Commit();
				if (SUCCEEDED(result))
					result = encoder->Commit();
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"IWICBitmapEncoder::Commit", result);
					return false;
				}
				return true;
			}

			[[nodiscard]] ID3D11Device* Device() const noexcept
			{
				return m_device.Get();
			}

			[[nodiscard]] ID3D11DeviceContext* Context() const noexcept
			{
				return m_context.Get();
			}

			[[nodiscard]] ID3D11Texture2D* BackBuffer() const noexcept
			{
				return m_backBuffer.Get();
			}

			[[nodiscard]] ID3D11RenderTargetView* BackBufferView() const noexcept
			{
				return m_backBufferView.Get();
			}

			[[nodiscard]] uint32_t Height() const noexcept
			{
				return m_height;
			}

		private:
			[[nodiscard]] HRESULT CreateDevice(
				D3D_DRIVER_TYPE a_driverType,
				DXGI_SWAP_CHAIN_DESC a_description) noexcept
			{
				return D3D11CreateDeviceAndSwapChain(
					nullptr,
					a_driverType,
					nullptr,
					D3D11_CREATE_DEVICE_BGRA_SUPPORT,
					nullptr,
					0,
					D3D11_SDK_VERSION,
					&a_description,
					m_swapChain.GetAddressOf(),
					m_device.GetAddressOf(),
					nullptr,
					m_context.GetAddressOf());
			}

			[[nodiscard]] bool CreateBackBuffer(std::wstring& a_error)
			{
				auto result = m_swapChain->GetBuffer(
					0,
					IID_PPV_ARGS(m_backBuffer.GetAddressOf()));
				if (SUCCEEDED(result))
					result = m_device->CreateRenderTargetView(
						m_backBuffer.Get(),
						nullptr,
						m_backBufferView.GetAddressOf());
				if (FAILED(result))
				{
					SetHRESULTError(a_error, L"Create render target", result);
					return false;
				}
				D3D11_TEXTURE2D_DESC description{};
				m_backBuffer->GetDesc(&description);
				m_width = description.Width;
				m_height = description.Height;
				return true;
			}

			void Reset() noexcept
			{
				m_backBufferView.Reset();
				m_backBuffer.Reset();
				m_swapChain.Reset();
				m_context.Reset();
				m_device.Reset();
			}

			ComPtr<ID3D11Device> m_device;
			ComPtr<ID3D11DeviceContext> m_context;
			ComPtr<IDXGISwapChain> m_swapChain;
			ComPtr<ID3D11Texture2D> m_backBuffer;
			ComPtr<ID3D11RenderTargetView> m_backBufferView;
			uint32_t m_width{};
			uint32_t m_height{};
			uint32_t m_pendingWidth{};
			uint32_t m_pendingHeight{};
		};

		LRESULT CALLBACK WindowProcedure(
			HWND a_window,
			UINT a_message,
			WPARAM a_wparam,
			LPARAM a_lparam)
		{
			if (g_imguiBackendReady &&
				ImGui_ImplWin32_WndProcHandler(
					a_window,
					a_message,
					a_wparam,
					a_lparam))
				return 1;

			switch (a_message)
			{
			case WM_SIZE:
				if (g_renderer && a_wparam != SIZE_MINIMIZED)
				{
					g_renderer->RequestResize(
						static_cast<uint32_t>(LOWORD(a_lparam)),
						static_cast<uint32_t>(HIWORD(a_lparam)));
				}
				return 0;
			case WM_SYSCOMMAND:
				if ((a_wparam & 0xFFF0u) == SC_KEYMENU)
					return 0;
				break;
			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;
			default:
				break;
			}
			return DefWindowProcW(a_window, a_message, a_wparam, a_lparam);
		}

		class PreviewWindow final
		{
		public:
			~PreviewWindow()
			{
				if (m_window)
					DestroyWindow(m_window);
				if (m_instance)
					UnregisterClassW(kWindowClassName, m_instance);
			}

			[[nodiscard]] bool Create(
				uint32_t a_width,
				uint32_t a_height,
				bool a_headless,
				std::wstring& a_error)
			{
				m_instance = GetModuleHandleW(nullptr);
				WNDCLASSEXW windowClass{};
				windowClass.cbSize = sizeof(windowClass);
				windowClass.style = CS_CLASSDC;
				windowClass.lpfnWndProc = WindowProcedure;
				windowClass.hInstance = m_instance;
				windowClass.hCursor = LoadCursorW(
					nullptr,
					MAKEINTRESOURCEW(32512));
				windowClass.lpszClassName = kWindowClassName;
				if (!RegisterClassExW(&windowClass))
				{
					a_error = L"Could not register the preview window class.";
					return false;
				}

				const auto style = a_headless ?
					static_cast<DWORD>(WS_POPUP) :
					static_cast<DWORD>(WS_OVERLAPPEDWINDOW);
				RECT bounds{
					0,
					0,
					static_cast<LONG>(a_width),
					static_cast<LONG>(a_height)
				};
				if (!a_headless && !AdjustWindowRectEx(&bounds, style, FALSE, 0))
				{
					a_error = L"Could not size the preview window.";
					return false;
				}
				m_window = CreateWindowExW(
					0,
					kWindowClassName,
					L"DearModdingUI Preview",
					style,
					a_headless ? 0 : CW_USEDEFAULT,
					a_headless ? 0 : CW_USEDEFAULT,
					bounds.right - bounds.left,
					bounds.bottom - bounds.top,
					nullptr,
					nullptr,
					m_instance,
					nullptr);
				if (!m_window)
				{
					a_error = L"Could not create the preview window.";
					return false;
				}
				return true;
			}

			void Show() noexcept
			{
				ShowWindow(m_window, SW_SHOWDEFAULT);
				UpdateWindow(m_window);
			}

			[[nodiscard]] HWND Handle() const noexcept
			{
				return m_window;
			}

		private:
			HINSTANCE m_instance{};
			HWND m_window{};
		};

		void PumpMessages(bool& a_running) noexcept
		{
			MSG message{};
			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
			{
				if (message.message == WM_QUIT)
				{
					a_running = false;
					return;
				}
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
		}

		class Application final
		{
		public:
			explicit Application(Options a_options) :
				m_options(std::move(a_options))
			{}

			~Application()
			{
				g_imguiBackendReady = false;
				g_renderer = nullptr;
				if (m_dx11Initialized)
					ImGui_ImplDX11_Shutdown();
				if (m_win32Initialized)
					ImGui_ImplWin32_Shutdown();
				if (m_context)
				{
					CursorLoader::Shutdown();
					BackgroundBlur::ResetDeviceResources();
					m_fakeData.reset();
					ImGui::DestroyContext(m_context);
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
				if (m_options.screenshot)
					return RunCapture(error);
				return RunInteractive(error);
			}

		private:
			[[nodiscard]] bool Initialize(std::wstring& a_error)
			{
				ImGui_ImplWin32_EnableDpiAwareness();
				if (!m_window.Create(
						m_options.width,
						m_options.height,
						m_options.screenshot.has_value(),
						a_error) ||
					!m_renderer.Initialize(
						m_window.Handle(),
						m_options.width,
						m_options.height,
						a_error))
					return false;
				g_renderer = &m_renderer;

				IMGUI_CHECKVERSION();
				m_context = ImGui::CreateContext();
				if (!m_context)
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
				if (!m_options.screenshot && !ConfigureIni(io, a_error))
					return false;

				HostSettings::Initialize();
				DearModdingUI::Initialize();
				m_fakeData = std::make_unique<FakeData>();
				std::string registrationError;
				if (!m_fakeData->Register(registrationError))
				{
					a_error.assign(
						registrationError.begin(),
						registrationError.end());
					return false;
				}

				Theme::Initialize(m_window.Handle());
				CursorLoader::Initialize(m_window.Handle());
				if (!ImGui_ImplWin32_Init(m_window.Handle()))
				{
					a_error = L"ImGui_ImplWin32_Init failed.";
					return false;
				}
				m_win32Initialized = true;
				if (!ImGui_ImplDX11_Init(
						m_renderer.Device(),
						m_renderer.Context()))
				{
					a_error = L"ImGui_ImplDX11_Init failed.";
					return false;
				}
				m_dx11Initialized = true;
				if (!ImGui_ImplDX11_CreateDeviceObjects())
				{
					a_error = L"ImGui_ImplDX11_CreateDeviceObjects failed.";
					return false;
				}
				g_imguiBackendReady = true;

				if (!BeginBackendInitialization())
				{
					a_error = L"The host refused backend initialization.";
					return false;
				}
				CompleteBackendInitialization(m_context);
				if (!SelectInitialPage(a_error))
					return false;
				return ConfigureSidebar(a_error);
			}

			[[nodiscard]] bool ConfigureSidebar(std::wstring& a_error)
			{
				if (m_options.expandedMods)
				{
					const auto& clients = Navigation().clients;
					for (const auto& id : *m_options.expandedMods)
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
					m_options.sidebarOverride,
					m_options.expandedMods.has_value(),
					m_options.expandedMods ?
						std::span<const std::string>{ *m_options.expandedMods } :
						std::span<const std::string>{});
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
				m_iniPath = (directory / L"imgui.ini").string();
				a_io.IniFilename = m_iniPath.c_str();
				a_io.IniSavingRate = 10.0f;
				return true;
			}

			[[nodiscard]] bool SelectInitialPage(std::wstring& a_error)
			{
				if (m_options.page && m_options.hostPage)
				{
					a_error = L"Choose either --page or --host-page.";
					return false;
				}
				if (!m_options.page)
				{
					if (SetMenuVisible(true) == DMUI_RESULT_OK)
					{
						if (m_options.hostPage)
							ConfigurePreviewHostPage(*m_options.hostPage);
						return true;
					}
					a_error = L"Could not open the host menu.";
					return false;
				}

				const auto separator = m_options.page->find('/');
				const std::string_view clientId{
					m_options.page->data(),
					separator
				};
				const std::string_view pageId{
					m_options.page->data() + separator + 1,
					m_options.page->size() - separator - 1
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
				if (!m_renderer.ApplyResize(a_error) ||
					!Theme::PrepareFrame(m_renderer.Height()))
				{
					if (a_error.empty())
						a_error = L"Theme::PrepareFrame failed.";
					return false;
				}

				CursorLoader::PrepareFrame(IsMenuVisible());
				BackgroundBlur::BeginFrame();
				ImGui_ImplDX11_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();
				DrawDemandedOverlays();
				if (IsMenuVisible())
					DrawShell();
				ImGui::Render();

				m_renderer.Clear();
				BackgroundBlur::Render(
					m_renderer.Device(),
					m_renderer.Context(),
					m_renderer.BackBuffer(),
					m_renderer.BackBufferView());
				m_renderer.BindBackBuffer();
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
				ObserveFrame();
				return true;
			}

			[[nodiscard]] int RunCapture(std::wstring& a_error)
			{
				bool running{ true };
				for (uint32_t frame = 0; frame < m_options.frames; ++frame)
				{
					PumpMessages(running);
					if (!running)
					{
						a_error = L"Preview window closed before capture.";
						break;
					}
					if (!RenderFrame(a_error))
						break;
				}
				if (a_error.empty())
					(void)m_renderer.Capture(*m_options.screenshot, a_error);
				if (!a_error.empty())
				{
					std::wcerr << L"dmui-preview: " << a_error << L'\n';
					return 1;
				}
				std::wcout << L"Wrote " << m_options.screenshot->wstring() << L'\n';
				return 0;
			}

			[[nodiscard]] int RunInteractive(std::wstring& a_error)
			{
				m_window.Show();
				bool running{ true };
				while (running)
				{
					PumpMessages(running);
					if (!running)
						break;
					if (IsIconic(m_window.Handle()))
					{
						std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
						continue;
					}
					if (!RenderFrame(a_error) ||
						!m_renderer.Present(a_error))
						break;
				}
				if (!a_error.empty())
				{
					std::wcerr << L"dmui-preview: " << a_error << L'\n';
					return 1;
				}
				return 0;
			}

			Options m_options;
			PreviewWindow m_window;
			Renderer m_renderer;
			ImGuiContext* m_context{};
			std::unique_ptr<FakeData> m_fakeData;
			std::string m_iniPath;
			bool m_win32Initialized{};
			bool m_dx11Initialized{};
		};
	}
}

int main()
{
	using namespace DearModdingUIPreview;

	int argumentCount{};
	auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (!arguments)
	{
		std::wcerr << L"dmui-preview: could not read the command line.\n";
		return 1;
	}

	Options options;
	std::wstring error;
	const auto parsed = ParseOptions(
		argumentCount,
		arguments,
		options,
		error);
	LocalFree(arguments);
	if (!parsed)
	{
		std::wcerr << L"dmui-preview: " << error << L'\n';
		PrintUsage();
		return 2;
	}
	if (options.help)
	{
		PrintUsage();
		return 0;
	}

	const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(com))
	{
		SetHRESULTError(error, L"CoInitializeEx", com);
		std::wcerr << L"dmui-preview: " << error << L'\n';
		return 1;
	}
	const auto result = Application{ std::move(options) }.Run();
	CoUninitialize();
	return result;
}
