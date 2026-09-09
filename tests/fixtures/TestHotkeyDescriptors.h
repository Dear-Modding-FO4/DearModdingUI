#pragma once

#include <DearModdingUI/API.h>

#include <array>

namespace DmuiTests
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
			"dearmodding.tests.general.toggle-overlay",
			"Toggle DMUI test overlay",
			"Ctrl+Shift+F10",
			DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED
		},
		{
			"dearmodding.tests.general.delayed-toast",
			"Post delayed DMUI test notification",
			"Ctrl+Shift+F11",
			DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED
		},
		{
			"dearmodding.tests.general.host-input-inactive-probe",
			"HOST_INPUT_INACTIVE probe",
			"NONE",
			DMUI_HOTKEY_CONTEXT_HOST_INPUT_INACTIVE
		},
		{
			"dearmodding.tests.general.always-probe",
			"Optional ALWAYS probe",
			"NONE",
			DMUI_HOTKEY_CONTEXT_ALWAYS
		},
		{
			"dearmodding.tests.general.letter-probe",
			"Letter A parser probe",
			"NONE",
			DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED
		},
		{
			"dearmodding.tests.general.digit-probe",
			"Digit 7 parser probe",
			"NONE",
			DMUI_HOTKEY_CONTEXT_GAMEPLAY_UNOBSTRUCTED
		}
	} };
}
