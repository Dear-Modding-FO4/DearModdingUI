#pragma once

#include <DearModdingUI/API.h>
#include <Platform/rendering/ImGuiPlatformTargets.h>

namespace DearModdingUI
{
	[[nodiscard]] constexpr DMUI_Result SwapChainAttachmentResult(
		Addictol::ImguiPlatform::AttachmentResult a_result) noexcept
	{
		using enum Addictol::ImguiPlatform::AttachmentResult;
		switch (a_result)
		{
		case kAttached:
			return DMUI_RESULT_OK;
		case kNotReady:
			return DMUI_RESULT_HOST_NOT_READY;
		case kBusy:
			return DMUI_RESULT_RENDERER_BUSY;
		default:
			return DMUI_RESULT_SWAPCHAIN_REJECTED;
		}
	}
}
