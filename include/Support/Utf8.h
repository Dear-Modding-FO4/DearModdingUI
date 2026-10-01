#pragma once

#include <cstddef>
#include <cstdint>

namespace Support
{
	[[nodiscard]] inline bool IsValidUtf8(const char* a_text, size_t a_length) noexcept
	{
		if (!a_text)
			return false;
		for (size_t offset = 0; offset < a_length;)
		{
			const auto lead = static_cast<unsigned char>(a_text[offset]);
			if (lead <= 0x7Fu)
			{
				if (!lead)
					return false;
				++offset;
				continue;
			}
			size_t length{};
			uint32_t minimum{}, codePoint{};
			if ((lead & 0xE0u) == 0xC0u)
			{
				length = 2; minimum = 0x80u; codePoint = lead & 0x1Fu;
			}
			else if ((lead & 0xF0u) == 0xE0u)
			{
				length = 3; minimum = 0x800u; codePoint = lead & 0x0Fu;
			}
			else if ((lead & 0xF8u) == 0xF0u)
			{
				length = 4; minimum = 0x10000u; codePoint = lead & 0x07u;
			}
			else
				return false;
			if (length > a_length - offset)
				return false;
			for (size_t index = 1; index < length; ++index)
			{
				const auto continuation = static_cast<unsigned char>(a_text[offset + index]);
				if ((continuation & 0xC0u) != 0x80u)
					return false;
				codePoint = (codePoint << 6u) | (continuation & 0x3Fu);
			}
			if (codePoint < minimum || codePoint > 0x10FFFFu ||
				(codePoint >= 0xD800u && codePoint <= 0xDFFFu))
				return false;
			offset += length;
		}
		return true;
	}
}
