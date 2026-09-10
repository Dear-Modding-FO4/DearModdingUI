#include <Platform/rendering/PlatformImGui.h>

namespace Addictol::PlatformImgui
{
	ImguiPlatform::AttachmentResult AttachSwapChain(IDXGISwapChain*) noexcept
	{
		return ImguiPlatform::AttachmentResult::kRejected;
	}

	bool QueryVideoMemory(uint64_t& a_used, uint64_t& a_budget) noexcept
	{
		a_used = 0;
		a_budget = 0;
		return false;
	}
}
