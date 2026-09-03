#include <DearModdingUI/MCM/TextMarkup.h>

#include <charconv>

namespace DearModdingUI::MCM
{
	namespace
	{
		[[nodiscard]] constexpr char Lower(char a_character) noexcept
		{
			return a_character >= 'A' && a_character <= 'Z' ?
				static_cast<char>(a_character - 'A' + 'a') :
				a_character;
		}

		[[nodiscard]] bool EqualsIgnoreCase(
			std::string_view a_left,
			std::string_view a_right) noexcept
		{
			if (a_left.size() != a_right.size())
				return false;
			for (size_t index = 0; index < a_left.size(); ++index)
			{
				if (Lower(a_left[index]) != Lower(a_right[index]))
					return false;
			}
			return true;
		}

		[[nodiscard]] constexpr bool IsSpace(char a_character) noexcept
		{
			return a_character == ' ' ||
				a_character == '\t' ||
				a_character == '\r' ||
				a_character == '\n';
		}

		[[nodiscard]] constexpr bool IsNameCharacter(
			char a_character) noexcept
		{
			return (a_character >= 'a' && a_character <= 'z') ||
				(a_character >= 'A' && a_character <= 'Z') ||
				(a_character >= '0' && a_character <= '9') ||
				a_character == '-' ||
				a_character == '_';
		}

		[[nodiscard]] std::string_view Trim(
			std::string_view a_text) noexcept
		{
			while (!a_text.empty() && IsSpace(a_text.front()))
				a_text.remove_prefix(1);
			while (!a_text.empty() && IsSpace(a_text.back()))
				a_text.remove_suffix(1);
			return a_text;
		}

		[[nodiscard]] std::optional<TextAlignment> ParseAlignment(
			std::string_view a_alignment) noexcept
		{
			a_alignment = Trim(a_alignment);
			if (EqualsIgnoreCase(a_alignment, "left"))
				return TextAlignment::kLeft;
			if (EqualsIgnoreCase(a_alignment, "center"))
				return TextAlignment::kCenter;
			if (EqualsIgnoreCase(a_alignment, "right"))
				return TextAlignment::kRight;
			return std::nullopt;
		}

		[[nodiscard]] std::optional<TextAlignment> ParseParagraphAlignment(
			std::string_view a_attributes) noexcept
		{
			auto offset = size_t{};
			while (offset < a_attributes.size())
			{
				while (offset < a_attributes.size() &&
					(IsSpace(a_attributes[offset]) ||
						a_attributes[offset] == '/'))
					++offset;
				const auto nameBegin = offset;
				while (offset < a_attributes.size() &&
					IsNameCharacter(a_attributes[offset]))
					++offset;
				if (nameBegin == offset)
				{
					++offset;
					continue;
				}
				const auto name =
					a_attributes.substr(nameBegin, offset - nameBegin);
				while (offset < a_attributes.size() &&
					IsSpace(a_attributes[offset]))
					++offset;
				if (offset == a_attributes.size() ||
					a_attributes[offset] != '=')
					continue;
				++offset;
				while (offset < a_attributes.size() &&
					IsSpace(a_attributes[offset]))
					++offset;
				if (offset == a_attributes.size())
					break;

				std::string_view value;
				if (a_attributes[offset] == '\'' ||
					a_attributes[offset] == '"')
				{
					const auto quote = a_attributes[offset++];
					const auto valueBegin = offset;
					while (offset < a_attributes.size() &&
						a_attributes[offset] != quote)
						++offset;
					value = a_attributes.substr(
						valueBegin,
						offset - valueBegin);
					if (offset < a_attributes.size())
						++offset;
				}
				else
				{
					const auto valueBegin = offset;
					while (offset < a_attributes.size() &&
						!IsSpace(a_attributes[offset]) &&
						a_attributes[offset] != '/')
						++offset;
					value = a_attributes.substr(
						valueBegin,
						offset - valueBegin);
				}

				if (EqualsIgnoreCase(name, "align"))
					return ParseAlignment(value);
			}
			return std::nullopt;
		}

		void AppendCodePoint(std::string& a_result, uint32_t a_value)
		{
			if (a_value <= 0x7F)
			{
				a_result.push_back(static_cast<char>(a_value));
			}
			else if (a_value <= 0x7FF)
			{
				a_result.push_back(static_cast<char>(0xC0 | (a_value >> 6)));
				a_result.push_back(static_cast<char>(0x80 | (a_value & 0x3F)));
			}
			else if (a_value <= 0xFFFF)
			{
				a_result.push_back(static_cast<char>(0xE0 | (a_value >> 12)));
				a_result.push_back(static_cast<char>(
					0x80 | ((a_value >> 6) & 0x3F)));
				a_result.push_back(static_cast<char>(0x80 | (a_value & 0x3F)));
			}
			else
			{
				a_result.push_back(static_cast<char>(0xF0 | (a_value >> 18)));
				a_result.push_back(static_cast<char>(
					0x80 | ((a_value >> 12) & 0x3F)));
				a_result.push_back(static_cast<char>(
					0x80 | ((a_value >> 6) & 0x3F)));
				a_result.push_back(static_cast<char>(0x80 | (a_value & 0x3F)));
			}
		}

		[[nodiscard]] bool AppendEntity(
			std::string& a_result,
			std::string_view a_text,
			size_t& a_offset)
		{
			const auto end = a_text.find(';', a_offset + 1);
			if (end == std::string_view::npos || end - a_offset > 12)
				return false;
			const auto entity =
				a_text.substr(a_offset + 1, end - a_offset - 1);
			if (entity == "amp")
				a_result.push_back('&');
			else if (entity == "lt")
				a_result.push_back('<');
			else if (entity == "gt")
				a_result.push_back('>');
			else if (entity == "quot")
				a_result.push_back('"');
			else if (entity == "apos")
				a_result.push_back('\'');
			else if (!entity.empty() && entity.front() == '#')
			{
				auto digits = entity.substr(1);
				auto base = 10;
				if (!digits.empty() &&
					(digits.front() == 'x' || digits.front() == 'X'))
				{
					digits.remove_prefix(1);
					base = 16;
				}
				uint32_t value{};
				const auto [parsed, error] = std::from_chars(
					digits.data(),
					digits.data() + digits.size(),
					value,
					base);
				if (digits.empty() ||
					error != std::errc{} ||
					parsed != digits.data() + digits.size() ||
					value > 0x10FFFF ||
					(value >= 0xD800 && value <= 0xDFFF))
					return false;
				AppendCodePoint(a_result, value);
			}
			else
			{
				return false;
			}
			a_offset = end + 1;
			return true;
		}
	}

	TextPresentation ResolveTextPresentation(
		std::string_view a_text,
		bool a_html,
		std::optional<std::string_view> a_alignment)
	{
		TextPresentation result;
		if (const auto alignment =
				a_alignment ? ParseAlignment(*a_alignment) : std::nullopt)
			result.alignment = *alignment;
		if (!a_html)
		{
			result.text = a_text;
			return result;
		}

		result.text.reserve(a_text.size());
		auto offset = size_t{};
		auto paragraphBreak = false;
		const auto appendParagraphBreak = [&] {
			if (!result.text.empty() && result.text.back() != '\n')
				result.text.push_back('\n');
			paragraphBreak = false;
		};
		while (offset < a_text.size())
		{
			if (a_text[offset] == '&')
			{
				if (paragraphBreak)
					appendParagraphBreak();
				if (AppendEntity(result.text, a_text, offset))
					continue;
			}
			if (a_text[offset] != '<')
			{
				if (paragraphBreak)
					appendParagraphBreak();
				result.text.push_back(a_text[offset++]);
				continue;
			}

			const auto end = a_text.find('>', offset + 1);
			if (end == std::string_view::npos)
			{
				if (paragraphBreak)
					appendParagraphBreak();
				result.text.push_back(a_text[offset++]);
				continue;
			}
			auto tag = Trim(a_text.substr(offset + 1, end - offset - 1));
			auto closing = false;
			if (!tag.empty() && tag.front() == '/')
			{
				closing = true;
				tag.remove_prefix(1);
				tag = Trim(tag);
			}
			if (tag.empty() ||
				tag.find('<') != std::string_view::npos ||
				!((tag.front() >= 'a' && tag.front() <= 'z') ||
					(tag.front() >= 'A' && tag.front() <= 'Z')))
			{
				if (paragraphBreak)
					appendParagraphBreak();
				result.text.push_back(a_text[offset++]);
				continue;
			}

			auto nameEnd = size_t{};
			while (nameEnd < tag.size() && IsNameCharacter(tag[nameEnd]))
				++nameEnd;
			const auto name = tag.substr(0, nameEnd);
			const auto attributes = tag.substr(nameEnd);
			offset = end + 1;
			if (EqualsIgnoreCase(name, "br") && !closing)
			{
				if (paragraphBreak)
					appendParagraphBreak();
				else
					result.text.push_back('\n');
			}
			else if (EqualsIgnoreCase(name, "p"))
			{
				if (closing)
				{
					paragraphBreak = !result.text.empty();
				}
				else
				{
					if (!result.text.empty())
						appendParagraphBreak();
					if (const auto alignment =
							ParseParagraphAlignment(attributes))
						result.alignment = *alignment;
				}
			}
		}
		return result;
	}
}
