#include <DearModdingUI/host/HeldInput.h>

#include <DearModdingUI/host/ControllerNavigation.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/PointerRouting.h>

namespace DearModdingUI
{
	size_t ReleaseHeldInput() noexcept
	{
		const auto released = Hotkeys::ReleaseActiveKeys();
		ControllerNavigation::Reset();
		PointerRouting::Reset();
		return released;
	}
}
