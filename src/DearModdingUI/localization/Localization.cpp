#include <DearModdingUI/localization/Localization.h>
#include <Platform/localization/TranslationFiles.h>
#include <Support/Runtime.h>

#include <REX/REX.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace DearModdingUI::Localization
{
	using namespace std::literals;

	namespace
	{
		struct TextHash
		{
			using is_transparent = void;

			[[nodiscard]] size_t operator()(std::string_view a_text) const noexcept
			{
				return std::hash<std::string_view>{}(a_text);
			}
		};

		template <class Value>
		using TextMap = std::unordered_map<std::string, Value, TextHash, std::equal_to<>>;
		using TranslationTable = TextMap<std::string>;

		struct OwnerTables
		{
			const TranslationTable* language{ nullptr };
			const TranslationTable* english{ nullptr };
		};

		// Tables are never freed, so returned strings live for the process.
		struct Catalog
		{
			std::mutex mutex;
			std::string languageOverride;
			std::string language;
			TextMap<OwnerTables> owners;
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
				a_catalog.language = a_catalog.languageOverride.empty() ?
					ReadGameLanguage() :
					a_catalog.languageOverride;
			}
			return !a_catalog.language.empty();
		}

		[[nodiscard]] const TranslationTable* LoadTable(
			Catalog& a_catalog,
			std::string_view a_owner,
			std::string_view a_language)
		{
			const auto name = std::format("{}_{}", a_owner, a_language);
			const auto path = std::filesystem::path{ Addictol::Support::GetRuntimeDirectory() } /
				"Data/Interface/Translations" / (name + ".txt");
			auto table = std::make_unique<const TranslationTable>(
				ParseTranslations(ReadTranslationFile(path).value_or(std::string{})));
			if (!table->empty())
				REX::INFO("DearModdingUI: loaded {} translations from {}.txt"sv, table->size(), name);
			return a_catalog.tables.emplace_back(std::move(table)).get();
		}

		[[nodiscard]] const OwnerTables& OwnerLocked(Catalog& a_catalog, std::string_view a_owner)
		{
			if (const auto found = a_catalog.owners.find(a_owner); found != a_catalog.owners.end())
				return found->second;
			OwnerTables tables{ LoadTable(a_catalog, a_owner, a_catalog.language) };
			tables.english = a_catalog.language == "en"sv ?
				tables.language :
				LoadTable(a_catalog, a_owner, "en"sv);
			return a_catalog.owners.emplace(std::string{ a_owner }, tables).first->second;
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

	void SetLanguage(std::string_view a_language) noexcept
	{
		try
		{
			auto& catalog = Instance();
			const std::scoped_lock lock{ catalog.mutex };
			catalog.languageOverride = a_language;
			catalog.language.clear();
			catalog.owners.clear();
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

	const std::string* FindTranslation(std::string_view a_owner, std::string_view a_key) noexcept
	{
		try
		{
			auto& catalog = Instance();
			const std::scoped_lock lock{ catalog.mutex };
			if (!ValidOwner(a_owner) || !IsTextKey(a_key) || !EnsureLanguageLocked(catalog))
				return nullptr;
			const auto& owner = OwnerLocked(catalog, a_owner);
			for (const auto* table : { owner.language, owner.english })
			{
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
}
