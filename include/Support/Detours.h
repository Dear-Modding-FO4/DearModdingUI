#pragma once

#include <cstdint>

namespace Addictol::Support
{
	[[nodiscard]] uintptr_t DetourVTable(
		uintptr_t a_target,
		uintptr_t a_function,
		uint32_t a_index) noexcept;
}
