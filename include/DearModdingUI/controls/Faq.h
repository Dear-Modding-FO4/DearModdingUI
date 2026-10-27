#pragma once

#include <DearModdingUI/API.h>

#include <cstddef>
#include <span>
#include <string_view>

namespace DearModdingUI
{
	struct FaqRowEntry
	{
		std::string_view question;
		std::string_view answer;
		// Caller-owned state the row toggles.
		bool* expanded{};
	};

	void DrawFaq(
		const char* a_id,
		std::span<const FaqRowEntry> a_entries) noexcept;

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
			if (!entry.question || !entry.question[0] ||
				!entry.answer || !entry.answer[0] || !entry.expanded)
				return DMUI_RESULT_INVALID_ARGUMENT;
		}
		return DMUI_RESULT_OK;
	}
}
