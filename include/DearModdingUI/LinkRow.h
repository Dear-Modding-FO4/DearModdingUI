#pragma once

#include <DearModdingUI/API.h>

namespace DearModdingUI
{
	[[nodiscard]] inline DMUI_Result DMUI_CALL ValidateLinkRowArguments(
		DMUI_ClientHandle,
		const char* a_id,
		const DMUI_LinkDescriptor* a_links,
		size_t a_count) noexcept
	{
		if (!a_id || (!a_links && a_count != 0))
			return DMUI_RESULT_INVALID_ARGUMENT;
		for (size_t index = 0; index < a_count; ++index)
		{
			const auto& link = a_links[index];
			if (link.structSize < DMUI_LINK_DESCRIPTOR_0_1_SIZE ||
				!link.label || !link.label[0] ||
				(link.enabled != 0 && (!link.url || !link.url[0])))
				return DMUI_RESULT_INVALID_ARGUMENT;
		}
		return DMUI_RESULT_OK;
	}
}
