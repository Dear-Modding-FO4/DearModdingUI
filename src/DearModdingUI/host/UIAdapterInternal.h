#pragma once

#include <DearModdingUI/host/UIAdapter.h>
#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace DearModdingUI::UI::AdapterInternal
{
	[[nodiscard]] DMUI_Result Validate(DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result CopyText(
		const char* a_text,
		size_t a_length,
		std::string& a_copy) noexcept;
	[[nodiscard]] DMUI_Result ValidateBuffer(
		char* a_buffer,
		uint32_t a_capacity) noexcept;
	[[nodiscard]] DMUI_Result ValidateScalar(
		DMUI_UIDataType a_type,
		void* a_data,
		uint32_t a_dataSize,
		const void* a_first,
		uint32_t a_firstSize,
		const void* a_second,
		uint32_t a_secondSize) noexcept;
	[[nodiscard]] ImVec2 Native(DMUI_Vec2 a_value) noexcept;
	[[nodiscard]] DMUI_Result TranslateInputFlags(
		DMUI_UIInputTextFlags a_flags,
		ImGuiInputTextFlags& a_native) noexcept;
}
