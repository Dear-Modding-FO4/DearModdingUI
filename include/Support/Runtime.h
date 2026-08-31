#pragma once

#include <string>

namespace Addictol::Support
{
	[[nodiscard]] std::string GetRuntimePath() noexcept;
	[[nodiscard]] std::string GetRuntimeDirectory() noexcept;
}
