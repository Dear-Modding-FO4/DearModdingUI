#pragma once

#include <cstdint>
#include <RE/P/PC_GAMEPAD_TYPE.h>

namespace RE
{
	class ControlMap
	{
	public:
		[[nodiscard]] static ControlMap* GetSingleton() noexcept
		{
			static ControlMap instance;
			return &instance;
		}

		PC_GAMEPAD_TYPE pcGamePadMapType{ PC_GAMEPAD_TYPE::kDirectX };
	};
}
