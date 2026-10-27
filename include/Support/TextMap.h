#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace DearModdingUI::Support
{
	struct TextHash
	{
		using is_transparent = void;

		[[nodiscard]] size_t operator()(std::string_view a_text) const noexcept
		{
			return std::hash<std::string_view>{}(a_text);
		}
	};

	// Lookups by string_view allocate no key.
	template <class Value>
	using TextMap = std::unordered_map<std::string, Value, TextHash, std::equal_to<>>;
}
