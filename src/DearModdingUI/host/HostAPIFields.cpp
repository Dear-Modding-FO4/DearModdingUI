#include "HostAPIEntries.h"
#include "HostContext.h"

#include <DearModdingUI/controls/SettingsTable.h>

#include <string_view>

namespace DearModdingUI::HostAPIInternal
{
	using namespace HostInternal;

	[[nodiscard]] DMUI_Result DMUI_CALL ApiBeginField(
		DMUI_ClientHandle a_client,
		const char* a_id,
		const char* a_label,
		const char* a_description,
		const DMUI_FieldBeginOptions* a_options,
		uint32_t* a_visible) noexcept
	{
		if (!a_visible)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_visible = 0u;
		const auto optionsValidation =
			SettingsTable::ValidateFieldBeginOptions(a_options);
		if (optionsValidation != DMUI_RESULT_OK)
			return optionsValidation;
		if (!a_id || a_id[0] == '\0')
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsFieldClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;

		const auto result = SettingsTable::BeginField(
			a_client,
			a_id,
			a_label,
			a_description,
			a_options->layout == DMUI_FIELD_LAYOUT_FULL_SPAN ?
				SettingsTable::FieldLayout::kFullSpan :
				SettingsTable::FieldLayout::kLabelValue);
		*a_visible = result.visible ? 1u : 0u;
		return result.result;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiSetFieldFeedback(
		DMUI_ClientHandle a_client,
		const DMUI_FieldFeedback* a_feedback) noexcept
	{
		size_t messageLength{};
		const auto feedbackValidation =
			SettingsTable::ValidateFieldFeedback(
				a_feedback,
				&messageLength);
		if (feedbackValidation != DMUI_RESULT_OK)
			return feedbackValidation;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsFieldClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;

		const std::string_view message{
			a_feedback->message ? a_feedback->message : "",
			messageLength
		};
		return SettingsTable::SetFieldFeedback(
			a_client,
			a_feedback->severity,
			message);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiEndField(
		DMUI_ClientHandle a_client,
		const DMUI_FieldEndOptions* a_options,
		uint32_t* a_resetPressed) noexcept
	{
		if (!a_resetPressed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_resetPressed = 0u;
		const auto optionsValidation =
			SettingsTable::ValidateFieldEndOptions(a_options);
		if (optionsValidation != DMUI_RESULT_OK)
			return optionsValidation;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsFieldClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;
		bool resetPressed{};
		const auto result = SettingsTable::EndField(
			a_client,
			{
				a_options->resetVisible != 0,
				a_options->resetEnabled != 0
			},
			resetPressed);
		if (result == DMUI_RESULT_OK)
			*a_resetPressed = resetPressed ? 1u : 0u;
		return result;
	}
}
