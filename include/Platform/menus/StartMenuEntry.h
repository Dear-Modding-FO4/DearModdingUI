#pragma once

namespace DearModdingUI::StartMenuEntry
{
	// Adds the host row to the start menus; pressing it calls a_open.
	[[nodiscard]] bool Register(void (*a_open)() noexcept) noexcept;
}
