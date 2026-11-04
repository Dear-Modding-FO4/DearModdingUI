#pragma once

#include <cstddef>

namespace DearModdingUI
{
	// Ends held hotkeys, controller and pointer presses; returns hotkeys freed.
	size_t ReleaseHeldInput() noexcept;
}
