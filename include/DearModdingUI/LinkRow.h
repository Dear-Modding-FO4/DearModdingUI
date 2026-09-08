#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/ExternalOpen.h>

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
				!link.label || !link.label[0] || link.reserved != 0 ||
				(link.action != DMUI_LINK_ACTION_COPY_TARGET &&
					link.action != DMUI_LINK_ACTION_OPEN_EXTERNAL))
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (link.enabled == 0)
				continue;
			ExternalOpenRequest request;
			const auto external =
				ValidateExternalOpenDescriptor(link.external, request);
			if (external != DMUI_RESULT_OK)
				return external;
			if (link.action == DMUI_LINK_ACTION_COPY_TARGET &&
				request.targetKind == DMUI_EXTERNAL_TARGET_NONE)
				return DMUI_RESULT_INVALID_DESCRIPTOR;
		}
		return DMUI_RESULT_OK;
	}
}
