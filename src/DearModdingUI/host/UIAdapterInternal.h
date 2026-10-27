#pragma once

#include <DearModdingUI/host/UIAdapter.h>
#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace DearModdingUI::UI::AdapterInternal
{
	[[nodiscard]] DMUI_Result Validate(DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] bool HasWindowDrawListClip(bool a_table = false) noexcept;
	[[nodiscard]] DMUI_Result EndDrawWindow(DMUI_ClientHandle a_client, void (*a_end)(), bool a_table = false) noexcept;
	[[nodiscard]] DMUI_Result ValidateText(
		DMUI_ClientHandle a_client, const char* a_text, size_t a_length) noexcept;
	[[nodiscard]] DMUI_Result CopyText(
		const char* a_text,
		size_t a_length,
		std::string& a_copy) noexcept;
	[[nodiscard]] ImVec2 Native(DMUI_Vec2 a_value) noexcept;
	[[nodiscard]] ImVec4 Native(DMUI_Vec4 a_value) noexcept;
	[[nodiscard]] DMUI_Vec2 Stable(ImVec2 a_value) noexcept;
	[[nodiscard]] bool Finite(const DMUI_Placement& a_placement) noexcept;
	[[nodiscard]] DMUI_Result TranslateInputFlags(
		DMUI_UIInputTextFlags a_flags,
		ImGuiInputTextFlags& a_native) noexcept;
}
