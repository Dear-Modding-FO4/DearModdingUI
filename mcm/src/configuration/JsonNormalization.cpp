#include <DearModdingUI/MCM/JsonNormalization.h>

namespace DearModdingUI::MCM
{
	namespace
	{
		[[nodiscard]] bool IsHex(char a_character) noexcept
		{
			return (a_character >= '0' && a_character <= '9') ||
				(a_character >= 'a' && a_character <= 'f') ||
				(a_character >= 'A' && a_character <= 'F');
		}

		[[nodiscard]] bool IsValidEscape(
			std::string_view a_json,
			size_t a_slash) noexcept
		{
			if (a_slash + 1 >= a_json.size())
				return false;
			const auto escaped = a_json[a_slash + 1];
			if (escaped == '"' || escaped == '\\' || escaped == '/' ||
				escaped == 'b' || escaped == 'f' || escaped == 'n' ||
				escaped == 'r' || escaped == 't')
				return true;
			if (escaped != 'u' || a_slash + 5 >= a_json.size())
				return false;
			for (size_t index = a_slash + 2; index <= a_slash + 5; ++index)
			{
				if (!IsHex(a_json[index]))
					return false;
			}
			return true;
		}

	}

	std::string NormalizeJson(
		std::string_view a_json,
		JsonNormalizationOptions a_options)
	{
		std::string result;
		result.reserve(a_json.size());
		auto inString = false;
		for (size_t index = 0; index < a_json.size(); ++index)
		{
			const auto current = a_json[index];
			if (inString)
			{
				if (current == '\\')
				{
					if (a_options.invalidEscapePassThrough &&
						!IsValidEscape(a_json, index))
						result.push_back('\\');
					result.push_back(current);
					if (index + 1 < a_json.size())
						result.push_back(a_json[++index]);
					continue;
				}
				result.push_back(current);
				if (current == '"')
					inString = false;
				continue;
			}

			if (current == '"')
			{
				inString = true;
				result.push_back(current);
				continue;
			}
			result.push_back(current);
		}
		return result;
	}
}
