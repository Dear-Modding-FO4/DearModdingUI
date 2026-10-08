#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace DearModdingUI
{
	// Lowercase sLanguage such as "en"; empty outside the game.
	[[nodiscard]] std::string ReadGameLanguage() noexcept;

	// A translation file's text as UTF-8, whether stored as UTF-8 or UTF-16 LE.
	[[nodiscard]] std::optional<std::string> ReadTranslationFile(
		const std::filesystem::path& a_path) noexcept;
}
