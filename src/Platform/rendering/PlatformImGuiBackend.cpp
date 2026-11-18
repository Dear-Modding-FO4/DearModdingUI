// CommonLib declarations must precede the Windows SDK macros.
#include <RE/B/BSGraphics.h>
#include <RE/M/MenuCursor.h>
#include <REX/REX.h>

#include "PlatformImGuiInternal.h"
#include <Platform/imgui/ImGuiWin32Integration.h>

#include <DearModdingUI/presentation/BackgroundBlur.h>
#include <Platform/menus/HostMenu.h>
#include <Platform/input/CursorLoader.h>
#include <Platform/input/EngineFocus.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/ControllerNavigation.h>
#include <DearModdingUI/host/PointerRouting.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/presentation/Theme.h>
#include <Platform/rendering/D3D11State.h>
#include <Support/OnceFlag.h>
#include <Support/Runtime.h>
#include <Support/ProcessLifetime.h>
#include <Support/SubsystemHealth.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>

#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#ifndef IMGUI_HAS_DOCK
#error "DearModdingUI requires Dear ImGui docking support"
#endif

static_assert(IMGUI_VERSION_NUM == 19300);
static_assert(std::string_view{ IMGUI_VERSION } == "1.93.0");

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandlerEx(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam,
	ImGuiIO& io);

namespace DearModdingUI::PlatformImGuiDetail
{
	using namespace std::literals;
	using namespace PlatformImGui;

	namespace
	{
		// The target the UI pass has bound when the host draws.
		struct FrameTarget
		{
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
			uint32_t width{ 0 };
			uint32_t height{ 0 };
		};

		struct BackendState
		{
			uint32_t targetWidth{ 0 };
			uint32_t targetHeight{ 0 };
			bool coordinateSpaceLogged{ false };
			Support::OnceFlag targetMissingLogged;
			std::string iniPath;
		};

		Support::ProcessLifetime<BackendState> s_backendStorage;
		BackendState& s_backendState = s_backendStorage.value;

		class RendererHealthReporter final :
			public DearModdingUI::HealthReporter
		{
		public:
			void Report(
				[[maybe_unused]] DearModdingUI::HealthEvent a_event,
				const DearModdingUI::HealthSnapshot& a_snapshot) noexcept override
			{
				if (a_snapshot.state == DearModdingUI::HealthState::kReady)
					REX::INFO("[dmui.render] Platform Imgui: renderer state: ready"sv);
				else if (a_snapshot.state == DearModdingUI::HealthState::kFailed)
					REX::ERROR("[dmui.render] Platform Imgui: renderer state failed: {}"sv, a_snapshot.reason.English());
			}
		};

		RendererHealthReporter s_rendererHealthReporter;
		DearModdingUI::SubsystemHealth s_rendererHealth{
			"dmui.render",
			{ "$DMUI_Health_SubsystemRenderer", "Renderer" },
			s_rendererHealthReporter,
			DearModdingUI::HostSubsystemHealthRegistry()
		};

		struct ClientSize
		{
			uint32_t width{ 0 };
			uint32_t height{ 0 };
		};

		[[nodiscard]] HWND GameWindow() noexcept
		{
			return Context().window.load(std::memory_order_acquire);
		}

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

		[[nodiscard]] Microsoft::WRL::ComPtr<IDXGIAdapter3> AcquireVideoMemoryAdapter(
			ID3D11Device* a_device) noexcept
		{
			Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
			Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
			Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
			if (FAILED(a_device->QueryInterface(
					IID_PPV_ARGS(dxgiDevice.ReleaseAndGetAddressOf()))) ||
				FAILED(dxgiDevice->GetParent(
					IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) ||
				FAILED(adapter->QueryInterface(
					IID_PPV_ARGS(adapter3.ReleaseAndGetAddressOf()))))
				return {};
			return adapter3;
		}

		// The engine creates its device once and never replaces it.
		[[nodiscard]] bool CaptureRenderDeviceLocked() noexcept
		{
			const auto* renderer = RE::BSGraphics::GetRendererData();
			const auto* window = RE::BSGraphics::GetCurrentRendererWindow();
			if (!renderer || !renderer->initialized || !renderer->device ||
				!renderer->context || !window || !window->hwnd)
			{
				REX::ERROR(
					"Platform Imgui: the game renderer has no complete device, context and window"sv);
				return false;
			}
			auto& context = Context();
			auto& render = context.render;
			render.device = reinterpret_cast<ID3D11Device*>(renderer->device);
			render.context = reinterpret_cast<ID3D11DeviceContext*>(renderer->context);
			render.videoMemoryAdapter = AcquireVideoMemoryAdapter(render.device.Get());
			context.window.store(reinterpret_cast<HWND>(window->hwnd), std::memory_order_release);
			return true;
		}

		[[nodiscard]] bool InitializeBackendLocked() noexcept
		{
			if (!CaptureRenderDeviceLocked())
				return false;
			auto& context = Context();
			const auto& render = context.render;
			const auto window = GameWindow();

			auto* imguiContext = ImGui::CreateContext();
			if (!imguiContext)
			{
				REX::ERROR(
					"Platform Imgui: ImGui::CreateContext() failed"sv);
				return false;
			}

			auto& io = ImGui::GetIO();
			io.ConfigFlags |=
				ImGuiConfigFlags_NavEnableKeyboard |
				ImGuiConfigFlags_NavEnableGamepad |
				ImGuiConfigFlags_DockingEnable;
			io.IniFilename = nullptr;
			io.MouseDrawCursor = false;
			ConfigureIniPath(io);
			context.callbacks.setup(window);

			if (!ImGui_ImplWin32_Init(window))
			{
				ImGui::DestroyContext(imguiContext);
				REX::ERROR(
					"Platform Imgui: ImGui_ImplWin32_Init() failed"sv);
				return false;
			}

			if (!ImGui_ImplDX11_Init(
					render.device.Get(),
					render.context.Get()))
			{
				ImGui_ImplWin32_Shutdown();
				ImGui::DestroyContext(imguiContext);
				REX::ERROR(
					"Platform Imgui: ImGui_ImplDX11_Init() failed"sv);
				return false;
			}

			if (!ImGui_ImplDX11_CreateDeviceObjects())
			{
				ImGui_ImplDX11_Shutdown();
				ImGui_ImplWin32_Shutdown();
				ImGui::DestroyContext(imguiContext);
				REX::ERROR(
					"Platform Imgui: D3D11 device-object creation failed"sv);
				return false;
			}

			DearModdingUI::CursorLoader::Initialize(
				window, DearModdingUI::CursorLoader::Source::kGame);
			DearModdingUI::PresentationServices::SetDevice(
				render.device.Get());
			REX::INFO(
				"Platform Imgui: ImGui initialized on the game's device"sv);
			return true;
		}

		// Clients register through kGameLoaded, sent just before this exit.
		[[nodiscard]] bool GameDataThreadFinished() noexcept
		{
			const auto thread = Context().gameDataThread.load(std::memory_order_acquire);
			return thread && WaitForSingleObject(thread, 0) == WAIT_OBJECT_0;
		}

		// Registration closes on the first pass after the game-data thread has exited.
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

			if (!GameDataThreadFinished() || !DearModdingUI::BeginBackendInitialization())
				return false;

			const auto ready = InitializeBackendLocked();
			context.backend.store(
				ready ? Backend::kReady : Backend::kFailed,
				std::memory_order_release);
			if (!ready)
			{
				CloseModalStateLocked(
					DearModdingUI::HostMenu::Event::kBackendFailure);
				DearModdingUI::FailBackendInitialization();
				(void)s_rendererHealth.Observe(
					DearModdingUI::HealthState::kFailed,
					Support::Message{ Support::Phrase{
						"$DMUI_Renderer_BackendFailed",
						"Dear ImGui could not be initialized on the game's renderer." } });
				return false;
			}
			DearModdingUI::CompleteBackendInitialization(
				ImGui::GetCurrentContext());
			(void)s_rendererHealth.Observe(DearModdingUI::HealthState::kReady, {});
			context.callbacks.ready();
			return true;
		}

		[[nodiscard]] std::optional<FrameTarget> ReadBoundTargetLocked() noexcept
		{
			FrameTarget target;
			Context().render.context->OMGetRenderTargets(
				1, target.view.GetAddressOf(), nullptr);
			if (!target.view)
				return std::nullopt;
			Microsoft::WRL::ComPtr<ID3D11Resource> resource;
			target.view->GetResource(resource.GetAddressOf());
			if (!resource || FAILED(resource.As(&target.texture)))
				return std::nullopt;
			D3D11_TEXTURE2D_DESC description{};
			target.texture->GetDesc(std::addressof(description));
			if (!description.Width || !description.Height)
				return std::nullopt;
			target.width = description.Width;
			target.height = description.Height;
			return target;
		}

		[[nodiscard]] std::optional<MousePosition> NativeCursorPositionLocked(
			const FrameTarget& a_target) noexcept
		{
			const auto* cursor = RE::MenuCursor::GetSingleton();
			const auto client = ReadClientSize(GameWindow());
			if (!cursor || !cursor->registeredCursors || !client.width || !client.height)
				return std::nullopt;
			return MapNativeCursorToTarget(
				{ static_cast<float>(cursor->cursorPosX), static_cast<float>(cursor->cursorPosY) },
				client.width, client.height, a_target.width, a_target.height);
		}

		void ApplyCoordinateSpaceLocked(
			std::optional<MousePosition> a_nativePosition) noexcept
		{
			auto& io = ImGui::GetIO();
			const auto window = GameWindow();
			const auto client = ReadClientSize(window);
			io.DisplaySize = {
				static_cast<float>(s_backendState.targetWidth),
				static_cast<float>(s_backendState.targetHeight)
			};
			if (a_nativePosition)
			{
				DearModdingUI::CursorLoader::ApplyNativePosition(
					a_nativePosition->x, a_nativePosition->y);
			}
			else if (!DearModdingUI::EngineFocus::Active())
			{
				constexpr auto unavailable = -(std::numeric_limits<float>::max)();
				DearModdingUI::CursorLoader::ApplyNativePosition(unavailable, unavailable);
			}
			else
			{
				const auto mouse = MapClientToTarget(
					ReadClientMousePosition(window),
					client.width, client.height,
					s_backendState.targetWidth,
					s_backendState.targetHeight);
				io.AddMousePosEvent(mouse.x, mouse.y);
			}
			if ((client.width != s_backendState.targetWidth ||
					client.height != s_backendState.targetHeight) &&
				!std::exchange(s_backendState.coordinateSpaceLogged, true))
			{
				REX::INFO(
					"Platform Imgui: client {}x{} differs from the UI target {}x{}; input is mapped to the target"sv,
					client.width,
					client.height,
					s_backendState.targetWidth,
					s_backendState.targetHeight);
			}
		}

		void ApplyCursorWarpLocked() noexcept
		{
			const auto target = DearModdingUI::ControllerNavigation::TakeCursorWarp();
			if (!target)
				return;
			auto* cursor = RE::MenuCursor::GetSingleton();
			const auto client = ReadClientSize(GameWindow());
			if (!cursor || !client.width || !client.height)
				return;
			const auto native = MapNativeCursorToTarget(
				{ target->x, target->y },
				s_backendState.targetWidth, s_backendState.targetHeight,
				client.width, client.height);
			// The engine setter also dispatches the position to registered Scaleform cursors.
			using SetCursorPos = void (*)(RE::MenuCursor*, int32_t, int32_t);
			static const REL::Relocation<SetCursorPos> setCursorPos{ REL::VariantID{ 865628, 2287488 } };
			setCursorPos(cursor, static_cast<int32_t>(std::lround(native.x)),
				static_cast<int32_t>(std::lround(native.y)));
			const auto position = MapNativeCursorToTarget(
				{ static_cast<float>(cursor->cursorPosX), static_cast<float>(cursor->cursorPosY) },
				client.width, client.height,
				s_backendState.targetWidth, s_backendState.targetHeight);
			DearModdingUI::CursorLoader::ApplyNativePosition(position.x, position.y);
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

		void SubmitFrameLocked(
			const FrameTarget& a_target,
			std::optional<MousePosition> a_nativePosition) noexcept
		{
			auto& context = Context();
			if (!DearModdingUI::Theme::PrepareFrame(a_target.height))
				return;
			const auto shellVisible = DearModdingUI::IsMenuVisible();
			DearModdingUI::BackgroundBlur::BeginFrame();
			DearModdingUI::PresentationServices::BeginFrame();

			ImGui_ImplDX11_NewFrame();
			// The engine queue is the sole gamepad producer in the game backend.
			auto& io = ImGui::GetIO();
			DearModdingUI::ImGuiWin32Integration::NewFrameWithoutGamepad();
			io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
			s_backendState.targetWidth = a_target.width;
			s_backendState.targetHeight = a_target.height;
			ApplyCoordinateSpaceLocked(a_nativePosition);
			DrainTextInputLocked();
			DearModdingUI::PointerRouting::BeginFrame(DearModdingUI::PointerOwnerPage());
			DearModdingUI::ControllerNavigation::PrepareFrame(DearModdingUI::CurrentInputMode());
			if (a_nativePosition)
				ApplyCursorWarpLocked();
			ImGui::NewFrame();
			context.callbacks.draw();
			ImGui::Render();
			PublishTextCapture(io.WantCaptureKeyboard);

			auto* deviceContext = context.render.context.Get();
			const PipelineState previousState{ deviceContext };
			if (shellVisible)
			{
				DearModdingUI::BackgroundBlur::Render(
					context.render.device.Get(),
					deviceContext,
					a_target.texture.Get(),
					a_target.view.Get());
			}
			deviceContext->OMSetRenderTargets(1, a_target.view.GetAddressOf(), nullptr);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			DearModdingUI::PresentationServices::CompleteRenderSubmission();
		}
	}

	void InitializeRendererHealth() noexcept
	{
		try
		{
			(void)s_rendererHealth.Observe(
				DearModdingUI::HealthState::kWaiting,
				Support::Message{ Support::Phrase{
					"$DMUI_Renderer_WaitingForGameData",
					"Waiting for the game to finish loading its data." } });
		}
		catch (...)
		{
			// Health is advisory; drawing does not depend on it.
		}
	}

	void DrawUiPassLocked() noexcept
	{
		if (!BackendReadyLocked())
			return;

		const auto modalVisible =
			DearModdingUI::CurrentInputMode() != DearModdingUI::HostInputMode::kGameplay;
		const auto overlayDemanded =
			DearModdingUI::NeedsFrame() && !modalVisible;
		ApplyDrawingRequestLocked(modalVisible);
		if (!ShouldRenderHostFrame(modalVisible, overlayDemanded))
			return;
		// Only a focused modal follows the game cursor.
		const auto drawing = Context().drawingEnabled.load(std::memory_order_acquire);

		const auto target = ReadBoundTargetLocked();
		if (!target)
		{
			if (s_backendState.targetMissingLogged.First())
				REX::WARN("Platform Imgui: the game's UI pass has no bound render target; frames are skipped"sv);
			return;
		}
		SubmitFrameLocked(*target, drawing ? NativeCursorPositionLocked(*target) : std::nullopt);
	}

	MousePosition SamplePointerLocked() noexcept
	{
		const auto* cursor = RE::MenuCursor::GetSingleton();
		const auto client = ReadClientSize(GameWindow());
		if (cursor && cursor->registeredCursors && client.width && client.height &&
			s_backendState.targetWidth && s_backendState.targetHeight)
		{
			return MapNativeCursorToTarget(
				{ static_cast<float>(cursor->cursorPosX), static_cast<float>(cursor->cursorPosY) },
				client.width, client.height,
				s_backendState.targetWidth, s_backendState.targetHeight);
		}
		const auto& mouse = ImGui::GetIO().MousePos;
		return { mouse.x, mouse.y };
	}

	void RefreshInputLanguageLocked(HWND a_window, ImGuiIO& a_io) noexcept
	{
		// The backend's code page serves the IME path of ANSI windows.
		(void)ImGui_ImplWin32_WndProcHandlerEx(a_window, WM_INPUTLANGCHANGE, 0, 0, a_io);
	}

	bool QueryVideoMemoryLocked(uint64_t& a_used, uint64_t& a_budget) noexcept
	{
		const auto& adapter = Context().render.videoMemoryAdapter;
		if (!adapter)
			return false;
		DXGI_QUERY_VIDEO_MEMORY_INFO info{};
		if (FAILED(adapter->QueryVideoMemoryInfo(
				0,
				DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
				std::addressof(info))))
			return false;
		a_used = info.CurrentUsage;
		a_budget = info.Budget;
		return true;
	}
}
