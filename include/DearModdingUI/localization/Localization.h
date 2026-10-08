#pragma once

#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace DearModdingUI::Localization
{
	// Owner of the host's own translation files.
	inline constexpr std::string_view kHostOwner{ "DearModdingUI" };

	// Only a whole "$KEY" is a key; text merely containing '$' is literal.
	[[nodiscard]] bool IsTextKey(std::string_view a_text) noexcept;

	// Uses a_language instead of the game's sLanguage; for the preview.
	void SetLanguage(std::string_view a_language) noexcept;

	// The active language code, or empty until it is known.
	[[nodiscard]] std::string Language() noexcept;

	// From <owner>_<language>.txt, then <owner>_en.txt; null if neither.
	[[nodiscard]] const std::string* FindTranslation(
		std::string_view a_owner,
		std::string_view a_key) noexcept;

	// Unknown keys and literal text stay unchanged.
	void Translate(std::string& a_text, std::string_view a_owner);

	// a_english must be static; it is returned when no translation exists.
	[[nodiscard]] const char* Text(std::string_view a_key, const char* a_english) noexcept;

	// Shown translated on screen, English in reports.
	struct Phrase
	{
		std::string_view key;
		const char* english;
	};

	[[nodiscard]] inline const char* Text(const Phrase& a_phrase, bool a_localized = true) noexcept
	{
		return a_localized ? Text(a_phrase.key, a_phrase.english) : a_phrase.english;
	}

	// A malformed translated template falls back to the English one.
	template <class... Args>
	[[nodiscard]] std::string Format(
		std::string_view a_key,
		const char* a_english,
		const Args&... a_args)
	{
		try
		{
			return std::vformat(Text(a_key, a_english), std::make_format_args(a_args...));
		}
		catch (const std::format_error&)
		{
			return std::vformat(a_english, std::make_format_args(a_args...));
		}
	}

	template <class... Args>
	[[nodiscard]] std::string Format(const Phrase& a_phrase, bool a_localized, const Args&... a_args)
	{
		return a_localized ?
			Format(a_phrase.key, a_phrase.english, a_args...) :
			std::vformat(a_phrase.english, std::make_format_args(a_args...));
	}
}
