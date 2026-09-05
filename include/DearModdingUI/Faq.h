#pragma once

#include <DearModdingUI/API.h>

namespace DearModdingUI
{
	[[nodiscard]] inline DMUI_Result DMUI_CALL ValidateFaqArguments(
		DMUI_ClientHandle,
		const char* a_id,
		const DMUI_FaqEntry* a_entries,
		size_t a_count) noexcept
	{
		if (!a_id || (!a_entries && a_count != 0))
			return DMUI_RESULT_INVALID_ARGUMENT;
		for (size_t index = 0; index < a_count; ++index)
		{
			const auto& entry = a_entries[index];
			if (entry.structSize < DMUI_FAQ_ENTRY_0_1_SIZE ||
				!entry.question || !entry.question[0] ||
				!entry.answer || !entry.answer[0])
				return DMUI_RESULT_INVALID_ARGUMENT;
		}
		return DMUI_RESULT_OK;
	}
}
