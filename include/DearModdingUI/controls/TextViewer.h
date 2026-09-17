#pragma once

#include <DearModdingUI/API.h>

namespace DearModdingUI
{
	[[nodiscard]] DMUI_Result DrawTextView(
		DMUI_ClientHandle a_client,
		const DMUI_TextViewDescriptor& a_descriptor,
		DMUI_TextViewState& a_state);
}
