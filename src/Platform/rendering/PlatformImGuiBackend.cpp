#include "PlatformImGuiInternal.h"

#include <DearModdingUI/presentation/BackgroundBlur.h>
#include <Platform/input/CarrierMenu.h>
#include <Platform/input/CursorLoader.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/presentation/Theme.h>
#include <Platform/rendering/D3D11State.h>
#include <Support/Runtime.h>
#include <Support/ProcessLifetime.h>

#include <REX/REX.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>

#include <filesystem>
#include <limits>
#include <string>
#include <utility>

#ifndef IMGUI_HAS_DOCK
#error "DearModdingUI requires Dear ImGui docking support"
#endif

static_assert(IMGUI_VERSION_NUM == 19291);
static_assert(std::string_view{ IMGUI_VERSION } == "1.92.9b");

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandlerEx(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam,
	ImGuiIO& io);

namespace Addictol::platformImguiDetail
{
	using namespace std::literals;
	using namespace ImguiPlatform;

	namespace
	{
		struct BackendState
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferView;
			BackBufferIdentity backBufferIdentity;
			bool backBufferFailureLogged{ false };
			bool coordinateSpaceLogged{ false };
			std::string iniPath;
		};

		Support::ProcessLifetime<BackendState> s_backendStorage;
		BackendState& s_backendState = s_backendStorage.value;

		struct ClientSize
		{
			uint32_t width{ 0 };
			uint32_t height{ 0 };
		};

		[[nodiscard]] ClientSize ReadClientSize(HWND a_window) noexcept
		{
			RECT client{};
			if (!GetClientRect(a_window, std::addressof(client)) ||
				client.right <= client.left ||
				client.bottom <= client.top)
				return {};
			return {
				static_cast<uint32_t>(client.right - client.left),
				static_cast<uint32_t>(client.bottom - client.top)
			};
		}

		[[nodiscard]] MousePosition ReadClientMousePosition(
			HWND a_window) noexcept
		{
			POINT position{};
			if (!GetCursorPos(std::addressof(position)) ||
				!ScreenToClient(a_window, std::addressof(position)))
			{
				constexpr auto unavailable =
					-(std::numeric_limits<float>::max)();
				return { unavailable, unavailable };
			}
			return {
				static_cast<float>(position.x),
				static_cast<float>(position.y)
			};
		}

		void ConfigureIniPath(ImGuiIO& a_io) noexcept
		{
			std::error_code error;
			const std::filesystem::path directory{
				Support::GetRuntimeDirectory() +
				"Data\\F4SE\\Plugins\\DearModdingUI"
			};
			std::filesystem::create_directories(directory, error);
			if (error)
			{
				REX::WARN(
					"DearModdingUI: \"{}\" could not be created; window geometry is not persisted."sv,
					directory.string());
				return;
			}

			s_backendState.iniPath = (directory / "imgui.ini").string();
			a_io.IniFilename = s_backendState.iniPath.c_str();
			a_io.IniSavingRate = 10.0f;
		}

		[[nodiscard]] bool InitializeBackendLocked() noexcept
		{
			auto& context = Context();
			const auto& attachment = context.attachment;
			if (!attachment.device ||
				!attachment.context ||
				!attachment.window ||
				!context.windowReady.load(std::memory_order_acquire))
			{
				REX::ERROR(
					"Platform Imgui: the active swapchain has no complete render binding"sv);
				return false;
			}

			auto* imguiContext = ImGui::GetCurrentContext();
			const auto createdContext = imguiContext == nullptr;
			if (createdContext)
			{
				imguiContext = ImGui::CreateContext();
				if (!imguiContext)
				{
					REX::ERROR(
						"Platform Imgui: ImGui::CreateContext() failed"sv);
					return false;
				}

				auto& io = ImGui::GetIO();
				io.ConfigFlags |=
					ImGuiConfigFlags_NavEnableKeyboard |
					ImGuiConfigFlags_DockingEnable;
				io.IniFilename = nullptr;
				io.MouseDrawCursor = false;
				ConfigureIniPath(io);
				context.callbacks.setup(attachment.window);
			}

			if (!ImGui_ImplWin32_Init(attachment.window))
			{
				if (createdContext)
					ImGui::DestroyContext(imguiContext);
				REX::ERROR(
					"Platform Imgui: ImGui_ImplWin32_Init() failed"sv);
				return false;
			}

			if (!ImGui_ImplDX11_Init(
					attachment.device.Get(),
					attachment.context.Get()))
			{
				ImGui_ImplWin32_Shutdown();
				if (createdContext)
					ImGui::DestroyContext(imguiContext);
				REX::ERROR(
					"Platform Imgui: ImGui_ImplDX11_Init() failed"sv);
				return false;
			}

			if (!ImGui_ImplDX11_CreateDeviceObjects())
			{
				ImGui_ImplDX11_Shutdown();
				ImGui_ImplWin32_Shutdown();
				if (createdContext)
					ImGui::DestroyContext(imguiContext);
				REX::ERROR(
					"Platform Imgui: D3D11 device-object creation failed"sv);
				return false;
			}

			DearModdingUI::CursorLoader::Initialize(attachment.window);
			DearModdingUI::PresentationServices::SetDevice(
				attachment.device.Get());
			REX::INFO(
				"Platform Imgui: ImGui initialized on the active swapchain"sv);
			return true;
		}

		[[nodiscard]] bool BackendReadyLocked() noexcept
		{
			auto& context = Context();
			switch (context.backend.load(std::memory_order_acquire))
			{
			case Backend::kReady:
				return true;
			case Backend::kFailed:
				return false;
			default:
				break;
			}

			const auto firstInitialization =
				ImGui::GetCurrentContext() == nullptr;
			if (firstInitialization)
			{
				if (!DearModdingUI::BeginBackendInitialization())
					return false;
			}

			const auto ready = InitializeBackendLocked();
			context.backend.store(
				ready ? Backend::kReady : Backend::kFailed,
				std::memory_order_release);
			if (!ready)
			{
				CloseModalStateLocked(
					DearModdingUI::CarrierMenu::Event::kBackendFailure);
				if (firstInitialization)
					DearModdingUI::FailBackendInitialization();
				else
				{
					DearModdingUI::SetBackendUnavailable(
						DMUI_UNAVAILABLE_BACKEND_FAILED);
				}
			}
			else if (firstInitialization)
				DearModdingUI::CompleteBackendInitialization(
					ImGui::GetCurrentContext());
			if (ready)
				SetRendererReadyLocked();
			return ready;
		}

		[[nodiscard]] bool EnsureBackBufferLocked(
			IDXGISwapChain* a_swapChain) noexcept
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> candidate;
			if (FAILED(a_swapChain->GetBuffer(
					0,
					IID_PPV_ARGS(
						candidate.ReleaseAndGetAddressOf()))) ||
				!candidate)
				return false;

			D3D11_TEXTURE2D_DESC description{};
			candidate->GetDesc(std::addressof(description));
			const BackBufferIdentity identity{
				reinterpret_cast<uintptr_t>(candidate.Get()),
				description.Width,
				description.Height
			};
			const auto decision = DecideBackBuffer(
				s_backendState.backBufferIdentity,
				identity,
				s_backendState.backBufferView != nullptr);
			if (decision == BackBufferDecision::kSkip)
				return false;
			if (decision == BackBufferDecision::kKeep)
				return true;

			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
			if (FAILED(Context().attachment.device->CreateRenderTargetView(
					candidate.Get(),
					nullptr,
					view.GetAddressOf())) ||
				!view)
			{
				ReleaseBackBufferLocked();
				if (!std::exchange(
						s_backendState.backBufferFailureLogged,
						true))
				{
					REX::ERROR(
						"Platform Imgui: creating the active swapchain backbuffer view failed"sv);
				}
				return false;
			}

			ReleaseBackBufferLocked();
			s_backendState.backBuffer = std::move(candidate);
			s_backendState.backBufferView = std::move(view);
			s_backendState.backBufferIdentity = identity;
			s_backendState.backBufferFailureLogged = false;
			return true;
		}

		void ApplyBackBufferCoordinateSpaceLocked() noexcept
		{
			auto& io = ImGui::GetIO();
			const auto client = ReadClientSize(Context().attachment.window);
			const auto mouse = MapClientToBackBuffer(
				ReadClientMousePosition(Context().attachment.window),
				client.width,
				client.height,
				s_backendState.backBufferIdentity.width,
				s_backendState.backBufferIdentity.height);
			io.DisplaySize = {
				static_cast<float>(
					s_backendState.backBufferIdentity.width),
				static_cast<float>(
					s_backendState.backBufferIdentity.height)
			};
			io.AddMousePosEvent(mouse.x, mouse.y);
			if ((client.width !=
						s_backendState.backBufferIdentity.width ||
					client.height !=
						s_backendState.backBufferIdentity.height) &&
				!std::exchange(
					s_backendState.coordinateSpaceLogged,
					true))
			{
				REX::INFO(
					"Platform Imgui: client {}x{} differs from backbuffer {}x{}; input is mapped to the backbuffer"sv,
					client.width,
					client.height,
					s_backendState.backBufferIdentity.width,
					s_backendState.backBufferIdentity.height);
			}
		}

		[[nodiscard]] LPARAM MapMouseMoveToBackBufferLocked(
			HWND a_window,
			LPARAM a_lparam) noexcept
		{
			const auto client = ReadClientSize(a_window);
			const MousePosition position{
				static_cast<float>(
					static_cast<int16_t>(LOWORD(a_lparam))),
				static_cast<float>(
					static_cast<int16_t>(HIWORD(a_lparam)))
			};
			const auto mapped = MapClientToBackBuffer(
				position,
				client.width,
				client.height,
				s_backendState.backBufferIdentity.width,
				s_backendState.backBufferIdentity.height);
			if (mapped.x == position.x && mapped.y == position.y)
				return a_lparam;
			return MAKELPARAM(
				static_cast<int16_t>(mapped.x),
				static_cast<int16_t>(mapped.y));
		}

		struct PipelineState
		{
			explicit PipelineState(
				ID3D11DeviceContext* a_context) noexcept :
				context(a_context),
				renderTargets(a_context)
			{
				context->HSGetShader(
					hull.shader.GetAddressOf(),
					hull.instances.data(),
					std::addressof(hull.count));
				context->DSGetShader(
					domain.shader.GetAddressOf(),
					domain.instances.data(),
					std::addressof(domain.count));
				context->CSGetShader(
					compute.shader.GetAddressOf(),
					compute.instances.data(),
					std::addressof(compute.count));
			}

			~PipelineState() noexcept
			{
				renderTargets.Restore(context);
				context->HSSetShader(
					hull.shader.Get(),
					hull.instances.data(),
					hull.count);
				context->DSSetShader(
					domain.shader.Get(),
					domain.instances.data(),
					domain.count);
				context->CSSetShader(
					compute.shader.Get(),
					compute.instances.data(),
					compute.count);
			}

			PipelineState(const PipelineState&) = delete;
			PipelineState& operator=(const PipelineState&) = delete;

			ID3D11DeviceContext* context;
			DearModdingUI::Rendering::RenderTargetState renderTargets;
			DearModdingUI::Rendering::ShaderState<ID3D11HullShader> hull;
			DearModdingUI::Rendering::ShaderState<ID3D11DomainShader> domain;
			DearModdingUI::Rendering::ShaderState<ID3D11ComputeShader> compute;
		};
	}

	void ReleaseBackBufferLocked() noexcept
	{
		DearModdingUI::BackgroundBlur::InvalidateBackBuffer();
		s_backendState.backBufferView.Reset();
		s_backendState.backBuffer.Reset();
		s_backendState.backBufferIdentity = {};
	}

	void ResetBackBufferFailureLocked() noexcept
	{
		s_backendState.backBufferFailureLogged = false;
	}

	void ShutdownBackendLocked() noexcept
	{
		auto& context = Context();
		DearModdingUI::PresentationServices::InvalidateDevice();
		CloseModalStateLocked(
			DearModdingUI::CarrierMenu::Event::kShutdown);
		DearModdingUI::CursorLoader::Shutdown();
		if (context.backend.load(std::memory_order_acquire) ==
			Backend::kReady)
		{
			DearModdingUI::BackgroundBlur::ResetDeviceResources();
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
		}
		context.backend.store(
			Backend::kUninitialized,
			std::memory_order_release);
	}

	void DrawFrameLocked(IDXGISwapChain* a_swapChain) noexcept
	{
		auto& context = Context();
		if (!ShouldInitializeHost(
				context.windowReady.load(std::memory_order_acquire)))
		{
			DearModdingUI::CarrierMenu::Handle(
				DearModdingUI::CarrierMenu::Event::kOverlayOnly);
			return;
		}
		if (!BackendReadyLocked())
		{
			CloseModalStateLocked(
				DearModdingUI::CarrierMenu::Event::kBackendFailure);
			return;
		}

		const auto modalVisible = DearModdingUI::IsMenuVisible();
		const auto overlayDemanded =
			DearModdingUI::NeedsFrame() && !modalVisible;
		ApplyDrawingRequestLocked(modalVisible);
		if (!ShouldRenderHostFrame(modalVisible, overlayDemanded) ||
			!EnsureBackBufferLocked(a_swapChain))
			return;
		if (!DearModdingUI::Theme::PrepareFrame(
				s_backendState.backBufferIdentity.height))
			return;
		DearModdingUI::BackgroundBlur::BeginFrame();
		DearModdingUI::PresentationServices::BeginFrame();

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ApplyBackBufferCoordinateSpaceLocked();
		ImGui::NewFrame();
		context.callbacks.draw();
		ImGui::Render();

		const PipelineState previousState{
			context.attachment.context.Get()
		};
		if (modalVisible)
		{
			DearModdingUI::BackgroundBlur::Render(
				context.attachment.device.Get(),
				context.attachment.context.Get(),
				s_backendState.backBuffer.Get(),
				s_backendState.backBufferView.Get());
		}
		auto* renderTarget = s_backendState.backBufferView.Get();
		context.attachment.context->OMSetRenderTargets(
			1,
			std::addressof(renderTarget),
			nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		DearModdingUI::PresentationServices::CompleteRenderSubmission();
	}

	BackendMessageResult HandleBackendWindowMessageLocked(
		HWND a_window,
		UINT a_message,
		WPARAM a_wparam,
		LPARAM a_lparam,
		bool a_escapeConsumed) noexcept
	{
		auto& context = Context();
		BackendMessageResult result;
		const auto modalVisible =
			context.drawingEnabled.load(std::memory_order_acquire);
		if (!HandlesWindowMessage(
				a_window ==
					context.activeWindow.load(std::memory_order_acquire),
				modalVisible,
				context.backend.load(std::memory_order_acquire) ==
					Backend::kReady,
				ImGui::GetCurrentContext() != nullptr))
			return result;

		auto& io = ImGui::GetIO();
		const auto backendLparam = a_message == WM_MOUSEMOVE ?
			MapMouseMoveToBackBufferLocked(a_window, a_lparam) :
			a_lparam;
		result.result = ImGui_ImplWin32_WndProcHandlerEx(
			a_window,
			a_message,
			a_wparam,
			backendLparam,
			io);
		result.swallow = a_escapeConsumed ||
			SwallowsMessage(
				ClassifyMessage(a_message),
				io.WantCaptureMouse,
				io.WantCaptureKeyboard);
		result.handled = true;
		return result;
	}
}
