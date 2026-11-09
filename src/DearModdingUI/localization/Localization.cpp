#include <DearModdingUI/localization/Localization.h>
#include <Platform/localization/TranslationFiles.h>
#include <Support/Runtime.h>
#include <Support/TextMap.h>

#include <REX/REX.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

namespace DearModdingUI::Localization
{
	using namespace std::literals;

	namespace
	{
		using TranslationTable = Support::TextMap<std::string>;

		using OwnerTables = Support::TextMap<const TranslationTable*>;

		// Tables are never freed, so returned strings live for the process.
		struct Catalog
		{
			std::mutex mutex;
			std::string languageOverride;
			std::string language;
			// sLanguage holds its built-in default until the game reads its INIs.
			bool gameLanguageReadable{ false };
			// Each owner's table in the active language, and in English.
			OwnerTables translated;
			OwnerTables english;
			std::vector<std::unique_ptr<const TranslationTable>> tables;
		};

		[[nodiscard]] Catalog& Instance() noexcept
		{
			static Catalog catalog;
			return catalog;
		}

		[[nodiscard]] std::string_view TrimText(std::string_view a_text) noexcept
		{
			const auto begin = a_text.find_first_not_of(" \t\r");
			if (begin == std::string_view::npos)
				return {};
			return a_text.substr(begin, a_text.find_last_not_of(" \t\r") - begin + 1);
		}

		// The MCM translation format: "$KEY<tab or space>Text" per line.
		[[nodiscard]] TranslationTable ParseTranslations(std::string_view a_text)
		{
			TranslationTable table;
			while (!a_text.empty())
			{
				const auto lineEnd = a_text.find('\n');
				const auto line = TrimText(a_text.substr(0, lineEnd));
				a_text.remove_prefix(lineEnd == std::string_view::npos ? a_text.size() : lineEnd + 1);
				const auto split = line.find_first_of(" \t");
				if (split == std::string_view::npos || !IsTextKey(line.substr(0, split)))
					continue;
				table.try_emplace(std::string{ line.substr(0, split) }, TrimText(line.substr(split)));
			}
			return table;
		}

		// Owners name files, so they must not leave the Translations directory.
		[[nodiscard]] bool ValidOwner(std::string_view a_owner) noexcept
		{
			return !a_owner.empty() &&
				!a_owner.contains("..") &&
				std::ranges::none_of(a_owner, [](unsigned char a_character) {
					return a_character < ' ' || a_character == '/' || a_character == '\\' ||
						a_character == ':';
				});
		}

		[[nodiscard]] bool EnsureLanguageLocked(Catalog& a_catalog)
		{
			if (a_catalog.language.empty())
			{
				a_catalog.language = !a_catalog.languageOverride.empty() ?
					a_catalog.languageOverride :
					a_catalog.gameLanguageReadable ? ReadGameLanguage() : std::string{};
			}
			return !a_catalog.language.empty();
		}

		[[nodiscard]] const TranslationTable* LoadTable(
			Catalog& a_catalog,
			std::string_view a_owner,
			std::string_view a_language)
		{
			const auto name = std::format("{}_{}", a_owner, a_language);
			const auto path = std::filesystem::path{ Support::GetRuntimeDirectory() } /
				"Data/Interface/Translations" / (name + ".txt");
			auto table = std::make_unique<const TranslationTable>(
				ParseTranslations(ReadTranslationFile(path).value_or(std::string{})));
			if (!table->empty())
				REX::INFO("DearModdingUI: loaded {} translations from {}.txt"sv, table->size(), name);
			return a_catalog.tables.emplace_back(std::move(table)).get();
		}

		[[nodiscard]] const TranslationTable* TableLocked(
			Catalog& a_catalog,
			OwnerTables& a_tables,
			std::string_view a_owner,
			std::string_view a_language)
		{
			if (const auto found = a_tables.find(a_owner); found != a_tables.end())
				return found->second;
			const auto* table = LoadTable(a_catalog, a_owner, a_language);
			a_tables.emplace(std::string{ a_owner }, table);
			return table;
		}
	}

	bool IsTextKey(std::string_view a_text) noexcept
	{
		return a_text.size() > 1 &&
			a_text.front() == '$' &&
			std::ranges::none_of(a_text, [](unsigned char a_character) {
				return a_character <= ' ';
			});
	}

	void EnableGameLanguage() noexcept
	{
		try
		{
			auto& catalog = Instance();
			const std::scoped_lock lock{ catalog.mutex };
			catalog.gameLanguageReadable = true;
		}
		catch (...)
		{
		}
	}

	void SetLanguage(std::string_view a_language) noexcept
	{
		try
		{
			auto& catalog = Instance();
			const std::scoped_lock lock{ catalog.mutex };
			catalog.languageOverride = a_language;
			catalog.language.clear();
			catalog.translated.clear();
			catalog.english.clear();
		}
		catch (...)
		{
		}
	}

	std::string Language() noexcept
	{
		try
		{
			auto& catalog = Instance();
			const std::scoped_lock lock{ catalog.mutex };
			return EnsureLanguageLocked(catalog) ? catalog.language : std::string{};
		}
		catch (...)
		{
			return {};
		}
	}

	namespace
	{
		enum class Tables
		{
			kTranslated,
			kTranslatedThenEnglish,
			kEnglish
		};

		[[nodiscard]] const std::string* Lookup(
			std::string_view a_owner,
			std::string_view a_key,
			Tables a_tables) noexcept
		{
			try
			{
				auto& catalog = Instance();
				const std::scoped_lock lock{ catalog.mutex };
				if (!ValidOwner(a_owner) || !IsTextKey(a_key))
					return nullptr;
				// English needs no language, which is unknown while mods register.
				if (a_tables != Tables::kEnglish && !EnsureLanguageLocked(catalog))
					return nullptr;
				const auto english = [&] {
					return TableLocked(catalog, catalog.english, a_owner, "en"sv);
				};
				std::array<const TranslationTable*, 2> tables{};
				if (a_tables != Tables::kEnglish)
					tables[0] = catalog.language == "en"sv ?
						english() :
						TableLocked(catalog, catalog.translated, a_owner, catalog.language);
				if (a_tables != Tables::kTranslated)
					tables[1] = english();
				for (const auto* table : tables)
				{
					if (!table)
						continue;
					if (const auto found = table->find(a_key); found != table->end())
						return &found->second;
				}
				return nullptr;
			}
			catch (...)
			{
				return nullptr;
			}
		}
	}

	const std::string* FindTranslation(std::string_view a_owner, std::string_view a_key) noexcept
	{
		return Lookup(a_owner, a_key, Tables::kTranslatedThenEnglish);
	}

	const std::string* FindEnglish(std::string_view a_owner, std::string_view a_key) noexcept
	{
		return Lookup(a_owner, a_key, Tables::kEnglish);
	}

	void Translate(std::string& a_text, std::string_view a_owner)
	{
		if (const auto* translated = FindTranslation(a_owner, a_text))
			a_text = *translated;
	}

	const char* Text(std::string_view a_key, const char* a_english) noexcept
	{
		const auto* translated = FindTranslation(kHostOwner, a_key);
		return translated ? translated->c_str() : a_english;
	}

	const Support::TextCatalog& Catalog() noexcept
	{
		struct HostCatalog final : Support::TextCatalog
		{
			[[nodiscard]] std::optional<std::string_view> Find(
				std::string_view a_key) const noexcept override
			{
				// English text would pair with this language's plural category.
				const auto* translated = Lookup(kHostOwner, a_key, Tables::kTranslated);
				return translated ? std::optional<std::string_view>{ *translated } : std::nullopt;
			}

			[[nodiscard]] Support::PluralCategory Plural(uint64_t a_count) const noexcept override
			{
				return Support::PluralCategoryFor(Language(), a_count);
			}
		};
		static const HostCatalog catalog;
		return catalog;
	}
}
