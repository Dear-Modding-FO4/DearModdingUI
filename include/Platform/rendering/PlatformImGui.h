#pragma once

#include <Platform/rendering/ImGuiPlatformTargets.h>

struct IDXGISwapChain;

namespace Addictol
{
	// The vtable and window hooks stay installed because process-exit teardown order is unsafe.
	namespace PlatformImgui
	{
		struct Callbacks
		{
			void (*setup)(void* a_window) noexcept{ nullptr };
			void (*draw)() noexcept{ nullptr };
			// Returns true to consume the press, its repeats, and its matching release.
			bool (*toggle)(uint32_t a_virtualKey) noexcept{ nullptr };

			[[nodiscard]] constexpr bool Valid() const noexcept
			{
				return setup && draw && toggle;
			}
		};

		// The required callbacks are permanent. Setup runs on the render thread
		// before backend initialization and font upload.
		[[nodiscard]] bool InstallHooks(Callbacks a_callbacks) noexcept;

		// Called at kGameLoaded, after the render window exists.
		[[nodiscard]] bool InitializeWindow() noexcept;

		// Deliberate external override for a final or proxy game swapchain.
		[[nodiscard]] ImguiPlatform::AttachmentResult AttachSwapChain(
			IDXGISwapChain* a_swapChain) noexcept;

		void SetDrawingEnabled(bool a_enabled) noexcept;
		void HandleGameTransition() noexcept;

		[[nodiscard]] bool IsReady() noexcept;
		[[nodiscard]] bool QueryVideoMemory(uint64_t& a_used, uint64_t& a_budget) noexcept;
	}
}