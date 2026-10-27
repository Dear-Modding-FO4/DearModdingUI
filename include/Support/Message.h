#pragma once

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace DearModdingUI::Support
{
	// A catalog key and its static English template.
	struct Phrase
	{
		std::string_view key;
		const char* english;
	};

	// Catalog keys are key + One, Few, Many, or Other; English uses one and other.
	struct PluralPhrase
	{
		std::string_view key;
		const char* one;
		const char* other;
	};

	enum class PluralCategory : uint8_t
	{
		kOne,
		kFew,
		kMany,
		kOther
	};

	// CLDR integer rules for the game's sLanguage codes; others follow English.
	[[nodiscard]] constexpr PluralCategory PluralCategoryFor(
		std::string_view a_language,
		uint64_t a_count) noexcept
	{
		const auto ones = a_count % 10;
		const auto tens = a_count % 100;
		if (a_language == "ru" || a_language == "pl")
		{
			if (a_language == "ru" ? ones == 1 && tens != 11 : a_count == 1)
				return PluralCategory::kOne;
			if (ones >= 2 && ones <= 4 && (tens < 12 || tens > 14))
				return PluralCategory::kFew;
			return PluralCategory::kMany;
		}
		if (a_language == "fr" || a_language == "ptbr")
			return a_count <= 1 ? PluralCategory::kOne : PluralCategory::kOther;
		if (a_language == "ja" || a_language == "cn")
			return PluralCategory::kOther;
		return a_count == 1 ? PluralCategory::kOne : PluralCategory::kOther;
	}

	// Translated templates; Find returns nothing to keep the English text.
	class TextCatalog
	{
	public:
		TextCatalog() = default;
		virtual ~TextCatalog() = default;

		TextCatalog(const TextCatalog&) = delete;
		TextCatalog(TextCatalog&&) = delete;
		TextCatalog& operator=(const TextCatalog&) = delete;
		TextCatalog& operator=(TextCatalog&&) = delete;

		[[nodiscard]] virtual std::optional<std::string_view> Find(
			std::string_view a_key) const noexcept = 0;
		[[nodiscard]] virtual PluralCategory Plural(uint64_t a_count) const noexcept = 0;
	};

	// Reports, logs, and tests render through this catalog.
	[[nodiscard]] inline const TextCatalog& EnglishCatalog() noexcept
	{
		struct English final : TextCatalog
		{
			[[nodiscard]] std::optional<std::string_view> Find(
				std::string_view) const noexcept override
			{
				return std::nullopt;
			}

			[[nodiscard]] PluralCategory Plural(uint64_t a_count) const noexcept override
			{
				return PluralCategoryFor("en", a_count);
			}
		};
		static const English catalog;
		return catalog;
	}

	// Kept unrendered so each viewer formats it in its own language.
	class Message
	{
	public:
		Message() = default;

		template <class... Args>
		explicit Message(const Phrase& a_phrase, const Args&... a_args) :
			m_kind(Kind::kPhrase),
			m_key(a_phrase.key),
			m_english(a_phrase.english),
			m_parts{ Argument(a_args)... }
		{}

		[[nodiscard]] static Message Text(std::string a_text)
		{
			Message message;
			message.m_text = std::move(a_text);
			return message;
		}

		template <class... Args>
		[[nodiscard]] static Message Plural(
			const PluralPhrase& a_phrase,
			uint64_t a_count,
			const Args&... a_args)
		{
			Message message;
			message.m_kind = Kind::kPlural;
			message.m_key = a_phrase.key;
			message.m_english = a_phrase.one;
			message.m_englishOther = a_phrase.other;
			message.m_count = a_count;
			message.m_parts = { Argument(a_args)... };
			return message;
		}

		[[nodiscard]] static Message Join(
			std::vector<Message> a_parts,
			std::string_view a_separator)
		{
			Message message;
			message.m_kind = Kind::kJoin;
			message.m_parts = std::move(a_parts);
			message.m_text = a_separator;
			return message;
		}

		[[nodiscard]] bool empty() const noexcept
		{
			return m_kind == Kind::kText ? m_text.empty() :
				m_kind == Kind::kJoin ? m_parts.empty() :
				false;
		}

		[[nodiscard]] std::string Render(const TextCatalog& a_catalog) const
		{
			if (m_kind == Kind::kText)
				return m_text;
			if (m_kind == Kind::kJoin)
			{
				std::string joined;
				for (const auto& part : m_parts)
				{
					if (!joined.empty() && !part.empty())
						joined.append(m_text);
					joined.append(part.Render(a_catalog));
				}
				return joined;
			}

			std::vector<std::string> arguments;
			arguments.reserve(m_parts.size());
			for (const auto& part : m_parts)
				arguments.push_back(part.Render(a_catalog));
			if (const auto translated = FindTemplate(a_catalog))
			{
				if (auto filled = Fill(*translated, arguments))
					return std::move(*filled);
			}
			const auto english = EnglishTemplate();
			return Fill(english, arguments).value_or(std::string{ english });
		}

		[[nodiscard]] std::string English() const
		{
			return Render(EnglishCatalog());
		}

		bool operator==(const Message&) const = default;

	private:
		enum class Kind : uint8_t
		{
			kText,
			kPhrase,
			kPlural,
			kJoin
		};

		template <class Value>
		[[nodiscard]] static Message Argument(const Value& a_value)
		{
			if constexpr (std::is_same_v<Value, Message>)
				return a_value;
			else if constexpr (std::is_convertible_v<const Value&, std::string_view>)
				return Text(std::string{ std::string_view{ a_value } });
			else
				return Text(std::format("{}", a_value));
		}

		[[nodiscard]] std::string_view EnglishTemplate() const noexcept
		{
			return m_kind == Kind::kPlural &&
					PluralCategoryFor("en", m_count) != PluralCategory::kOne ?
				m_englishOther :
				m_english;
		}

		[[nodiscard]] std::optional<std::string_view> FindTemplate(
			const TextCatalog& a_catalog) const
		{
			if (m_kind == Kind::kPhrase)
				return a_catalog.Find(m_key);
			constexpr std::string_view kSuffixes[]{ "One", "Few", "Many", "Other" };
			auto key = std::string{ m_key };
			key.append(kSuffixes[static_cast<size_t>(a_catalog.Plural(m_count))]);
			if (const auto found = a_catalog.Find(key))
				return found;
			return a_catalog.Find(std::string{ m_key }.append("Other"));
		}

		// Nothing for a malformed template, so a translator typo shows English.
		[[nodiscard]] static std::optional<std::string> Fill(
			std::string_view a_template,
			const std::vector<std::string>& a_arguments)
		{
			std::string result;
			result.reserve(a_template.size());
			size_t next = 0;
			bool automatic = false;
			bool manual = false;
			for (size_t index = 0; index < a_template.size(); ++index)
			{
				const auto character = a_template[index];
				const auto doubled = index + 1 < a_template.size() &&
					a_template[index + 1] == character;
				if (character == '}')
				{
					if (!doubled)
						return std::nullopt;
					result.push_back('}');
					++index;
					continue;
				}
				if (character != '{')
				{
					result.push_back(character);
					continue;
				}
				if (doubled)
				{
					result.push_back('{');
					++index;
					continue;
				}
				const auto close = a_template.find('}', index + 1);
				if (close == std::string_view::npos)
					return std::nullopt;
				const auto field = a_template.substr(index + 1, close - index - 1);
				size_t argument = 0;
				if (field.empty())
				{
					automatic = true;
					argument = next++;
				}
				else
				{
					manual = true;
					for (const auto digit : field)
					{
						if (digit < '0' || digit > '9')
							return std::nullopt;
						argument = argument * 10 + static_cast<size_t>(digit - '0');
					}
				}
				if ((automatic && manual) || argument >= a_arguments.size())
					return std::nullopt;
				result.append(a_arguments[argument]);
				index = close;
			}
			return result;
		}

		Kind m_kind{ Kind::kText };
		std::string_view m_key;
		std::string_view m_english;
		std::string_view m_englishOther;
		uint64_t m_count{ 0 };
		std::vector<Message> m_parts;
		std::string m_text;
	};
}
