#pragma once

#include <Platform/input/EngineKeys.h>
#include <Platform/rendering/ImGuiPlatformTargets.h>

#include <cstdint>

struct IDXGISwapChain;

namespace DearModdingUI
{
	// The vtable and window hooks stay installed because process-exit teardown order is unsafe.
	namespace PlatformImGui
	{
		struct Callbacks
		{
			void (*setup)(void* a_window) noexcept{ nullptr };
			void (*draw)() noexcept{ nullptr };
			void (*toggle)() noexcept{ nullptr };

			[[nodiscard]] constexpr bool Valid() const noexcept
			{
				return setup && draw && toggle;
			}
		};

		// The required callbacks are permanent. Setup runs on the render thread
		// before backend initialization and font upload.
		[[nodiscard]] bool InstallHooks(Callbacks a_callbacks) noexcept;

		// Taps the calling thread's message loop; call on the game's main thread.
		[[nodiscard]] bool InstallThreadTap() noexcept;
		// Confirms the tapped thread owns the game window and reports health.
		[[nodiscard]] bool VerifyThreadTap() noexcept;

		// Logs the focus inputs so a wrong engine offset is visible.
		void LogEngineFocus(std::string_view a_context) noexcept;

		// Called at kGameLoaded, after the render window exists.
		[[nodiscard]] bool InitializeWindow() noexcept;

		// Deliberate external override for a final or proxy game swapchain.
		[[nodiscard]] PlatformImGui::AttachmentResult AttachSwapChain(
			IDXGISwapChain* a_swapChain) noexcept;

		void SetDrawingEnabled(bool a_enabled) noexcept;
		void HandleGameTransition() noexcept;
		enum class InputDevice : uint8_t
		{
			kKeyboard,
			kMouse,
			kGamepad
		};

		// One button edge from the engine input queue.
		struct ButtonEdge
		{
			InputDevice device{ InputDevice::kKeyboard };
			EngineKeys::Edge edge{ EngineKeys::Edge::kPress };
			// Keyboard: engine virtual key. Mouse and gamepad: KeyCatalog code.
			uint32_t id{ 0 };
			float value{ 1.0f };
			// Engine modifier state when the edge was polled.
			uint32_t modifiers{ 0 };
		};

		// True when DMUI acted on the edge, so the game must not see it.
		[[nodiscard]] bool ObserveButton(const ButtonEdge& a_edge) noexcept;
		// Releases holds the engine dropped; returns hotkey keys released.
		size_t ResetInput() noexcept;
		void ObserveStick(bool a_left, float a_x, float a_y) noexcept;
		void ObserveMouseMove() noexcept;
		void ReleaseGamepad() noexcept;

		[[nodiscard]] Readiness CurrentReadiness() noexcept;
		[[nodiscard]] bool IsReady() noexcept;
		[[nodiscard]] bool QueryVideoMemory(uint64_t& a_used, uint64_t& a_budget) noexcept;
	}
}