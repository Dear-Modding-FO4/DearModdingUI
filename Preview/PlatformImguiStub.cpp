#include <Platform/PlatformImgui.h>

namespace Addictol::PlatformImgui
{
	bool AttachSwapChain(IDXGISwapChain*) noexcept
	{
		return false;
	}

	bool QueryVideoMemory(uint64_t& a_used, uint64_t& a_budget) noexcept
	{
		a_used = 0;
		a_budget = 0;
		return false;
	}
}
