#pragma once

#include <Platform/rendering/ImGuiPlatformTargets.h>

#include <Windows.h>
#include <dxgi.h>

#undef ERROR

struct IDXGISwapChain;

namespace Addictol::platformImguiDetail
{
	using PresentHook = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
	using ResizeBuffersHook = HRESULT(WINAPI*)(
		IDXGISwapChain*,
		UINT,
		UINT,
		UINT,
		DXGI_FORMAT,
		UINT);

	class SwapChainHooks
	{
	public:
		[[nodiscard]] static bool Install(
			IDXGISwapChain* a_swapChain,
			ImguiPlatform::AttachmentLifecycle a_lifecycle,
			PresentHook a_presentHook,
			ResizeBuffersHook a_resizeBuffersHook) noexcept;

		[[nodiscard]] static PresentHook PreviousPresent(
			IDXGISwapChain* a_swapChain,
			PresentHook a_presentHook) noexcept;

		[[nodiscard]] static ResizeBuffersHook PreviousResizeBuffers(
			IDXGISwapChain* a_swapChain,
			ResizeBuffersHook a_resizeBuffersHook) noexcept;
	};
}
