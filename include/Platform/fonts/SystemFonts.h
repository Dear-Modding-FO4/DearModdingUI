#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace DearModdingUI
{
	struct SystemFontFace
	{
		std::string_view name;
		std::span<const std::byte> data;
	};

	// Mapped for the process; a_language orders CJK faces on the first call.
	[[nodiscard]] const std::vector<SystemFontFace>& SystemFallbackFonts(
		std::string_view a_language) noexcept;
}
