#include "UIAdapterInternal.h"

#include <DearModdingUI/UIBindings.generated.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace DearModdingUI::UI::Bindings
{
	using namespace AdapterInternal;

	DMUI_Result DMUI_CALL Button(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_Vec2 a_size,
		uint32_t* a_pressed) noexcept
	{
		if (!a_label || !a_pressed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_pressed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		*a_pressed = ImGui::Button(a_label, Native(a_size)) ? 1u : 0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL Checkbox(
		DMUI_ClientHandle a_client,
		const char* a_label,
		uint32_t* a_value,
		uint32_t* a_changed) noexcept
	{
		if (!a_label || !a_value || !a_changed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_changed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		bool value = *a_value != 0;
		*a_changed = ImGui::Checkbox(a_label, &value) ? 1u : 0u;
		*a_value = value ? 1u : 0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL CollapsingHeader(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_UITreeNodeFlags a_flags,
		uint32_t* a_open) noexcept
	{
		if (!a_label || !a_open)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_open = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		ImGuiTreeNodeFlags flags{};
		const auto translated = TranslateTreeNodeFlags(a_flags, flags);
		if (translated != DMUI_RESULT_OK)
			return translated;
		*a_open = ImGui::CollapsingHeader(a_label, flags) ? 1u : 0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL CollapsingHeaderVisible(
		DMUI_ClientHandle a_client,
		const char* a_label,
		uint32_t* a_visible,
		DMUI_UITreeNodeFlags a_flags,
		uint32_t* a_open) noexcept
	{
		if (!a_label || !a_visible || !a_open)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_open = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		ImGuiTreeNodeFlags flags{};
		const auto translated = TranslateTreeNodeFlags(a_flags, flags);
		if (translated != DMUI_RESULT_OK)
			return translated;
		bool visible = *a_visible != 0;
		*a_open = ImGui::CollapsingHeader(a_label, &visible, flags) ? 1u : 0u;
		*a_visible = visible ? 1u : 0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL DragScalar(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_UIDataType a_dataType,
		void* a_data,
		uint32_t a_dataSize,
		float a_speed,
		const void* a_minimum,
		uint32_t a_minimumSize,
		const void* a_maximum,
		uint32_t a_maximumSize,
		const char* a_format,
		DMUI_UISliderFlags a_flags,
		uint32_t* a_changed) noexcept
	{
		if (!a_label || !a_changed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_changed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		const auto scalar = ValidateScalar(
			a_dataType,
			a_data,
			a_dataSize,
			a_minimum,
			a_minimumSize,
			a_maximum,
			a_maximumSize);
		if (scalar != DMUI_RESULT_OK)
			return scalar;
		ImGuiDataType dataType{};
		ImGuiSliderFlags flags{};
		const auto typeResult = TranslateDataType(a_dataType, dataType);
		const auto flagResult = TranslateSliderFlags(a_flags, flags);
		if (typeResult != DMUI_RESULT_OK)
			return typeResult;
		if (flagResult != DMUI_RESULT_OK)
			return flagResult;
		*a_changed = ImGui::DragScalar(
			a_label,
			dataType,
			a_data,
			a_speed,
			a_minimum,
			a_maximum,
			a_format,
			flags) ?
			1u :
			0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL InputScalar(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_UIDataType a_dataType,
		void* a_data,
		uint32_t a_dataSize,
		const void* a_step,
		uint32_t a_stepSize,
		const void* a_fastStep,
		uint32_t a_fastStepSize,
		const char* a_format,
		DMUI_UIInputTextFlags a_flags,
		uint32_t* a_changed) noexcept
	{
		if (!a_label || !a_changed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_changed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		const auto scalar = ValidateScalar(
			a_dataType,
			a_data,
			a_dataSize,
			a_step,
			a_stepSize,
			a_fastStep,
			a_fastStepSize);
		if (scalar != DMUI_RESULT_OK)
			return scalar;
		ImGuiDataType dataType{};
		ImGuiInputTextFlags flags{};
		const auto typeResult = TranslateDataType(a_dataType, dataType);
		const auto flagResult = TranslateInputFlags(a_flags, flags);
		if (typeResult != DMUI_RESULT_OK)
			return typeResult;
		if (flagResult != DMUI_RESULT_OK)
			return flagResult;
		*a_changed = ImGui::InputScalar(
			a_label,
			dataType,
			a_data,
			a_step,
			a_fastStep,
			a_format,
			flags) ?
			1u :
			0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL InputText(
		DMUI_ClientHandle a_client,
		const char* a_label,
		char* a_buffer,
		uint32_t a_capacity,
		DMUI_UIInputTextFlags a_flags,
		uint32_t* a_changed) noexcept
	{
		if (!a_label || !a_changed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_changed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		const auto buffer = ValidateBuffer(a_buffer, a_capacity);
		if (buffer != DMUI_RESULT_OK)
			return buffer;
		ImGuiInputTextFlags flags{};
		const auto translated = TranslateInputFlags(a_flags, flags);
		if (translated != DMUI_RESULT_OK)
			return translated;
		*a_changed = ImGui::InputText(
			a_label, a_buffer, a_capacity, flags, nullptr, nullptr) ?
			1u :
			0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL InputTextMultiline(
		DMUI_ClientHandle a_client,
		const char* a_label,
		char* a_buffer,
		uint32_t a_capacity,
		DMUI_Vec2 a_size,
		DMUI_UIInputTextFlags a_flags,
		uint32_t* a_changed) noexcept
	{
		if (!a_label || !a_changed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_changed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		const auto buffer = ValidateBuffer(a_buffer, a_capacity);
		if (buffer != DMUI_RESULT_OK)
			return buffer;
		ImGuiInputTextFlags flags{};
		const auto translated = TranslateInputFlags(a_flags, flags);
		if (translated != DMUI_RESULT_OK)
			return translated;
		*a_changed = ImGui::InputTextMultiline(
			a_label,
			a_buffer,
			a_capacity,
			Native(a_size),
			flags,
			nullptr,
			nullptr) ?
			1u :
			0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL InputTextWithHint(
		DMUI_ClientHandle a_client,
		const char* a_label,
		const char* a_hint,
		char* a_buffer,
		uint32_t a_capacity,
		DMUI_UIInputTextFlags a_flags,
		uint32_t* a_changed) noexcept
	{
		if (!a_label || !a_hint || !a_changed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_changed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		const auto buffer = ValidateBuffer(a_buffer, a_capacity);
		if (buffer != DMUI_RESULT_OK)
			return buffer;
		ImGuiInputTextFlags flags{};
		const auto translated = TranslateInputFlags(a_flags, flags);
		if (translated != DMUI_RESULT_OK)
			return translated;
		*a_changed = ImGui::InputTextWithHint(
			a_label,
			a_hint,
			a_buffer,
			a_capacity,
			flags,
			nullptr,
			nullptr) ?
			1u :
			0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL ProgressBar(
		DMUI_ClientHandle a_client,
		float a_fraction,
		DMUI_Vec2 a_size,
		const char* a_overlay,
		size_t a_overlayLength) noexcept
	{
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		std::string overlay;
		const auto copied = CopyText(
			a_overlay, a_overlayLength, overlay);
		if (copied != DMUI_RESULT_OK)
			return copied;
		ImGui::ProgressBar(
			a_fraction,
			Native(a_size),
			overlay.empty() ? nullptr : overlay.c_str());
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL Selectable(
		DMUI_ClientHandle a_client,
		const char* a_label,
		uint32_t a_selected,
		DMUI_UISelectableFlags a_flags,
		DMUI_Vec2 a_size,
		uint32_t* a_pressed) noexcept
	{
		if (!a_label || !a_pressed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_pressed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		ImGuiSelectableFlags flags{};
		const auto translated = TranslateSelectableFlags(a_flags, flags);
		if (translated != DMUI_RESULT_OK)
			return translated;
		*a_pressed = ImGui::Selectable(
			a_label, a_selected != 0, flags, Native(a_size)) ?
			1u :
			0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL SelectableToggle(
		DMUI_ClientHandle a_client,
		const char* a_label,
		uint32_t* a_selected,
		DMUI_UISelectableFlags a_flags,
		DMUI_Vec2 a_size,
		uint32_t* a_pressed) noexcept
	{
		if (!a_label || !a_selected || !a_pressed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_pressed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		ImGuiSelectableFlags flags{};
		const auto translated = TranslateSelectableFlags(a_flags, flags);
		if (translated != DMUI_RESULT_OK)
			return translated;
		bool selected = *a_selected != 0;
		*a_pressed = ImGui::Selectable(
			a_label, &selected, flags, Native(a_size)) ?
			1u :
			0u;
		*a_selected = selected ? 1u : 0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL SliderScalar(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_UIDataType a_dataType,
		void* a_data,
		uint32_t a_dataSize,
		const void* a_minimum,
		uint32_t a_minimumSize,
		const void* a_maximum,
		uint32_t a_maximumSize,
		const char* a_format,
		DMUI_UISliderFlags a_flags,
		uint32_t* a_changed) noexcept
	{
		if (!a_label || !a_changed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_changed = 0u;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		const auto scalar = ValidateScalar(
			a_dataType,
			a_data,
			a_dataSize,
			a_minimum,
			a_minimumSize,
			a_maximum,
			a_maximumSize);
		if (scalar != DMUI_RESULT_OK)
			return scalar;
		ImGuiDataType dataType{};
		ImGuiSliderFlags flags{};
		const auto typeResult = TranslateDataType(a_dataType, dataType);
		const auto flagResult = TranslateSliderFlags(a_flags, flags);
		if (typeResult != DMUI_RESULT_OK)
			return typeResult;
		if (flagResult != DMUI_RESULT_OK)
			return flagResult;
		*a_changed = ImGui::SliderScalar(
			a_label,
			dataType,
			a_data,
			a_minimum,
			a_maximum,
			a_format,
			flags) ?
			1u :
			0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL PlotLines(
		DMUI_ClientHandle a_client,
		const char* a_label,
		const float* a_values,
		int32_t a_valueCount,
		int32_t a_valueOffset,
		const char* a_overlay,
		size_t a_overlayLength,
		float a_scaleMinimum,
		float a_scaleMaximum,
		DMUI_Vec2 a_size,
		uint32_t a_strideBytes) noexcept
	{
		if (!a_label ||
			!a_values ||
			a_valueCount < 0 ||
			a_valueOffset < 0 ||
			a_strideBytes < sizeof(float))
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		std::string overlay;
		const auto copied = CopyText(
			a_overlay, a_overlayLength, overlay);
		if (copied != DMUI_RESULT_OK)
			return copied;
		ImGui::PlotLines(
			a_label,
			a_values,
			a_valueCount,
			a_valueOffset,
			overlay.empty() ? nullptr : overlay.c_str(),
			a_scaleMinimum,
			a_scaleMaximum,
			Native(a_size),
			static_cast<int>(a_strideBytes));
		return DMUI_RESULT_OK;
	}
}
