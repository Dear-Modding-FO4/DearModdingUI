#include <Platform/rendering/PlatformImGui.h>

namespace DearModdingUI::PlatformImGui
{
	PlatformImGui::AttachmentResult AttachSwapChain(IDXGISwapChain*) noexcept
	{
		return PlatformImGui::AttachmentResult::kRejected;
	}

	bool QueryVideoMemory(uint64_t& a_used, uint64_t& a_budget) noexcept
	{
		a_used = 0;
		a_budget = 0;
		return false;
	}
}
