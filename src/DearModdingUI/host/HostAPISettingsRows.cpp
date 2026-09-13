#include "HostAPIEntries.h"
#include "HostContext.h"

#include <DearModdingUI/controls/SettingsTable.h>

namespace DearModdingUI::HostAPIInternal
{
	using namespace HostInternal;

	DMUI_Result DMUI_CALL ApiBeginSettingsRow(
		DMUI_ClientHandle a_client,
		const char* a_id,
		const char* a_label,
		const char* a_description,
		uint32_t* a_visible) noexcept
	{
		if (!a_id || !a_label || !a_visible)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_visible = 0u;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;

		const auto result =
			SettingsTable::BeginRow(a_client, a_id, a_label, a_description);
		*a_visible = result.visible ? 1u : 0u;
		return result.result;
	}

	DMUI_Result DMUI_CALL ApiBeginSettingsRowEx(
		DMUI_ClientHandle a_client,
		const char* a_id,
		const char* a_label,
		const char* a_description,
		const DMUI_SettingsRowBeginOptions* a_options,
		uint32_t* a_visible) noexcept
	{
		if (!a_visible)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_visible = 0u;
		const auto optionsValidation =
			SettingsTable::ValidateRowBeginOptions(a_options);
		if (optionsValidation != DMUI_RESULT_OK)
			return optionsValidation;
		if (!a_id || !a_label)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;

		const auto result = SettingsTable::BeginRow(
			a_client,
			a_id,
			a_label,
			a_description,
			a_options->layout == DMUI_SETTINGS_ROW_LAYOUT_FULL_SPAN ?
				SettingsTable::RowLayout::kFullSpan :
				SettingsTable::RowLayout::kLabelValue);
		*a_visible = result.visible ? 1u : 0u;
		return result.result;
	}

	DMUI_Result DMUI_CALL ApiEndSettingsRow(
		DMUI_ClientHandle a_client,
		const DMUI_SettingsRowOptions* a_options,
		uint32_t* a_resetPressed) noexcept
	{
		if (!a_resetPressed)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_resetPressed = 0u;
		const auto optionsValidation =
			SettingsTable::ValidateRowOptions(a_options);
		if (optionsValidation != DMUI_RESULT_OK)
			return optionsValidation;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!SettingsTable::AcceptsClient(a_client))
			return DMUI_RESULT_WRONG_THREAD;

		bool resetPressed{};
		const auto result = SettingsTable::EndRow(
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
