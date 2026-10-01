#pragma once

namespace DearModdingUI::ImGuiWin32Integration
{
	void NewFrameWithoutGamepad() noexcept;
	inline constexpr char kContextPropertyName[] =
		"DearModdingUI.IMGUI_CONTEXT";
	inline constexpr wchar_t kPlatformWindowClassName[] =
		L"DearModdingUI ImGui Platform";
}
