#include "UIAdapterInternal.h"

#include <DearModdingUI/UIBindings.generated.h>
#include <DearModdingUI/controls/Button.h>
#include <DearModdingUI/controls/Choice.h>
#include <DearModdingUI/controls/ColorEdit.h>
#include <DearModdingUI/controls/KeyCapture.h>
#include <DearModdingUI/controls/Numeric.h>
#include <DearModdingUI/controls/Spinner.h>
#include <DearModdingUI/controls/TextInput.h>
#include <DearModdingUI/controls/Toggle.h>

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
		const DMUI_ButtonDescriptor* a_button,
		uint32_t* a_pressed) noexcept
	{
		if (!a_pressed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_pressed = 0u;
		const auto described = ValidateButton(a_button);
		if (described != DMUI_RESULT_OK)
			return described;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		*a_pressed = DrawButton(MakeButtonSpec(*a_button)) ? 1u : 0u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL MeasureButton(
		DMUI_ClientHandle a_client,
		const DMUI_ButtonDescriptor* a_button,
		DMUI_Vec2* a_size) noexcept
	{
		if (!a_size)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_size = {};
		const auto described = ValidateButton(a_button);
		if (described != DMUI_RESULT_OK)
			return described;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		*a_size = Stable(DearModdingUI::MeasureButton(MakeButtonSpec(*a_button)));
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL Toggle(
		DMUI_ClientHandle a_client,
		const DMUI_ToggleDescriptor* a_toggle,
		DMUI_ToggleResult* a_result) noexcept
	{
		if (!a_result)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_result = {};
		const auto described = ValidateToggle(a_toggle);
		if (described != DMUI_RESULT_OK)
			return described;
		*a_result = { a_toggle->value != 0 ? 1u : 0u, 0u };
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		*a_result = DrawToggle(*a_toggle);
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL Spinner(
		DMUI_ClientHandle a_client,
		const DMUI_SpinnerDescriptor* a_spinner) noexcept
	{
		const auto described = ValidateSpinner(a_spinner);
		if (described != DMUI_RESULT_OK)
			return described;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		DrawSpinner(*a_spinner);
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL Choice(
		DMUI_ClientHandle a_client,
		const DMUI_ChoiceDescriptor* a_choice,
		DMUI_ChoiceResult* a_result) noexcept
	{
		if (!a_result)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_result = {};
		const auto described = ValidateChoice(a_choice);
		if (described != DMUI_RESULT_OK)
			return described;
		*a_result = { a_choice->selected, 0u, 0u };
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		*a_result = DrawChoice(*a_choice);
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL ColorEdit(
		DMUI_ClientHandle a_client,
		const DMUI_ColorEditDescriptor* a_color,
		DMUI_ColorEditResult* a_result) noexcept
	{
		if (!a_result)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_result = {};
		const auto described = ValidateColorEdit(a_color);
		if (described != DMUI_RESULT_OK)
			return described;
		*a_result = { a_color->rgba, 0u, 0u };
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		*a_result = DrawColorEdit(*a_color);
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL KeyCapture(
		DMUI_ClientHandle a_client,
		const DMUI_KeyCaptureDescriptor* a_capture,
		DMUI_KeyCaptureResult* a_result) noexcept
	{
		if (!a_result)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_result = {};
		const auto described = ValidateKeyCapture(a_capture);
		if (described != DMUI_RESULT_OK)
			return described;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		const auto captured = DrawKeyCapture(MakeKeyCaptureSpec(*a_capture));
		if (!captured)
			return DMUI_RESULT_OK;
		const auto chord = SerializeHotkeyChord(*captured);
		if (chord.size() >= sizeof(a_result->chord))
			return DMUI_RESULT_BUFFER_TOO_SMALL;
		std::memcpy(a_result->chord, chord.c_str(), chord.size() + 1u);
		a_result->changed = 1u;
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL Numeric(
		DMUI_ClientHandle a_client,
		const DMUI_NumericDescriptor* a_numeric,
		DMUI_NumericResult* a_result) noexcept
	{
		if (!a_result)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_result = {};
		const auto described = ValidateNumeric(a_numeric);
		if (described != DMUI_RESULT_OK)
			return described;
		*a_result = { a_numeric->value, 0u, 0u };
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		*a_result = DrawNumeric(*a_numeric);
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL InputText(
		DMUI_ClientHandle a_client,
		const char* a_id,
		DMUI_TextBuffer* a_buffer,
		const DMUI_TextFieldOptions* a_options,
		DMUI_TextEditState* a_state) noexcept
	{
		if (!a_buffer || !a_options || !a_state)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_state = {};
		if (a_options->reserved != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		TextFieldSpec spec{
			.hint = a_options->hint,
			.style = a_options->style,
			.editFlags = a_options->editFlags,
			.maxBytes = a_options->maxBytes,
			.lines = a_options->lines,
			.size = { a_options->size.x, a_options->size.y },
			.cursor = a_options->cursor,
			.styleSpans = a_options->styleSpans,
			.styleUserData = a_options->styleUserData
		};
		const auto translated = TranslateInputFlags(a_options->flags, spec.flags);
		if (translated != DMUI_RESULT_OK)
			return translated;
		return DrawTextField(a_id, *a_buffer, spec, *a_state);
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
