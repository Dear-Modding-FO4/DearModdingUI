#pragma once

#include <DearModdingUI/API.h>

#include <array>

namespace DmuiForwardingSmoke
{
	struct HotkeyDescriptor
	{
		const char* id;
		const char* name;
		const char* suggested;
		DMUI_HotkeyContextPolicy policy;
	};

	inline constexpr std::array<HotkeyDescriptor, 6> kHotkeyDescriptors{ {
		{
			"dearmodding.forwarding-smoke.toggle-overlay",
			"Toggle forwarding smoke overlay",
			"Ctrl+Shift+F10",
			DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED
		},
		{
			"dearmodding.forwarding-smoke.delayed-toast",
			"Post delayed forwarding smoke notification",
			"Ctrl+Shift+F11",
			DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED
		},
		{
			"dearmodding.forwarding-smoke.host-input-inactive-probe",
			"HOST_INPUT_INACTIVE probe",
			"NONE",
			DMUI_HOTKEY_CONTEXT_HOST_INPUT_INACTIVE
		},
		{
			"dearmodding.forwarding-smoke.always-probe",
			"Optional ALWAYS probe",
			"NONE",
			DMUI_HOTKEY_CONTEXT_ALWAYS
		},
		{
			"dearmodding.forwarding-smoke.letter-probe",
			"Letter A parser probe",
			"NONE",
			DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED
		},
		{
			"dearmodding.forwarding-smoke.digit-probe",
			"Digit 7 parser probe",
			"NONE",
			DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED
		}
	} };
}
