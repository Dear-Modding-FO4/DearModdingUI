#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace DearModdingUI::Internal
{
	[[nodiscard]] inline std::optional<std::string_view> ReadBoundedString(
		const char* a_value,
		size_t a_limit,
		bool a_optional) noexcept
	{
		if (!a_value)
			return a_optional ? std::optional{ std::string_view{} } : std::nullopt;

		size_t length{};
		while (length <= a_limit && a_value[length])
			++length;
		if (length > a_limit || (!a_optional && !length))
			return std::nullopt;
		return std::string_view{ a_value, length };
	}

	[[nodiscard]] inline bool CopyBoundedString(
		const char* a_value,
		size_t a_limit,
		bool a_optional,
		std::string& a_output)
	{
		const auto value = ReadBoundedString(a_value, a_limit, a_optional);
		if (!value)
			return false;
		if (a_value)
			a_output.assign(*value);
		return true;
	}

	[[nodiscard]] inline bool ValidText(
		std::string_view a_text,
		bool a_optional) noexcept
	{
		if (!a_optional && a_text.empty())
			return false;
		bool hasVisibleCharacter = false;
		for (const auto character : a_text)
		{
			if (static_cast<unsigned char>(character) < 0x20u &&
				character != '\t')
				return false;
			if (character != ' ' && character != '\t')
				hasVisibleCharacter = true;
		}
		return a_optional || hasVisibleCharacter;
	}
}
