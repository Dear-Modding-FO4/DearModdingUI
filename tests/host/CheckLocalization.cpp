#include "../Harness.h"
#include "../support/LanguageScope.h"

#include <DearModdingUI/localization/Localization.h>
#include <Platform/localization/TranslationFiles.h>
#include <Support/Runtime.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <optional>

namespace vmm_tests
{
	namespace
	{
		namespace Localization = DearModdingUI::Localization;
		namespace Support = DearModdingUI::Support;

		constexpr Support::PluralPhrase kErrorCount{ "$DMUI_Health_ErrorCount", "{} error", "{} errors" };

		using support::LanguageScope;

		[[nodiscard]] std::filesystem::path TranslationDirectory()
		{
			return std::filesystem::path{ Support::GetRuntimeDirectory() } /
				"Data/Interface/Translations";
		}

		// Shipped keys mapped to their number of replacement fields.
		[[nodiscard]] std::map<std::string, size_t> ShippedFields(std::string_view a_language)
		{
			const auto file = TranslationDirectory() /
				std::format("{}_{}.txt", Localization::kHostOwner, a_language);
			const auto text = DearModdingUI::ReadTranslationFile(file);
			require(text.has_value(), "missing " + file.string());

			const LanguageScope language{ a_language };
			std::map<std::string, size_t> fields;
			std::string_view remaining{ *text };
			while (!remaining.empty())
			{
				const auto end = remaining.find('\n');
				const auto line = remaining.substr(0, end);
				remaining.remove_prefix(end == std::string_view::npos ? remaining.size() : end + 1);
				const auto key = std::string{ line.substr(0, line.find_first_of(" \t")) };
				if (!Localization::IsTextKey(key))
					continue;
				const auto* value = Localization::FindTranslation(Localization::kHostOwner, key);
				require(value != nullptr, std::string{ a_language } + " does not resolve " + key);
				size_t count = 0;
				for (size_t index = 0; index < value->size(); ++index)
				{
					if ((*value)[index] == '{' && index + 1 < value->size() && (*value)[index + 1] == '{')
						++index;
					else if ((*value)[index] == '{')
						++count;
				}
				fields.emplace(key, count);
			}
			return fields;
		}

		[[nodiscard]] bool IsPluralVariant(
			const std::map<std::string, size_t>& a_english,
			std::string_view a_key)
		{
			for (const std::string_view suffix : { "Few", "Many" })
			{
				if (a_key.ends_with(suffix))
				{
					const auto base = std::string{ a_key.substr(0, a_key.size() - suffix.size()) };
					return a_english.contains(base + "One") && a_english.contains(base + "Other");
				}
			}
			return false;
		}

		class TemporaryTranslation
		{
		public:
			TemporaryTranslation(std::string_view a_name, std::string_view a_bytes) :
				m_path(TranslationDirectory() / a_name)
			{
				std::filesystem::create_directories(m_path.parent_path());
				std::ofstream file{ m_path, std::ios::binary | std::ios::trunc };
				file.write(a_bytes.data(), static_cast<std::streamsize>(a_bytes.size()));
				require(file.good(), "could not write " + m_path.string());
			}

			~TemporaryTranslation()
			{
				std::error_code error;
				std::filesystem::remove(m_path, error);
			}

			TemporaryTranslation(const TemporaryTranslation&) = delete;
			TemporaryTranslation& operator=(const TemporaryTranslation&) = delete;

		private:
			std::filesystem::path m_path;
		};

		// "$DMUI_Check_Greeting<tab>Privet {}" in Cyrillic, kept ASCII in source.
		constexpr char16_t kUtf16Greeting[]{
			u'$', u'D', u'M', u'U', u'I', u'_', u'C', u'h', u'e', u'c', u'k', u'_',
			u'G', u'r', u'e', u'e', u't', u'i', u'n', u'g', 0x0009,
			0x041F, 0x0440, 0x0438, 0x0432, 0x0435, 0x0442, u' ', u'{', u'}', 0x000D, 0x000A, 0
		};

		[[nodiscard]] std::string Utf16LittleEndian(std::u16string_view a_text)
		{
			std::string bytes{ "\xFF\xFE" };
			for (const auto unit : a_text)
			{
				bytes.push_back(static_cast<char>(unit & 0xFF));
				bytes.push_back(static_cast<char>(unit >> 8));
			}
			return bytes;
		}
	}

	void run_localization_checks(Runner& runner)
	{
		runner.test("shipped host translations match English keys and fields", [] {
			const auto english = ShippedFields("en");
			require(!english.empty(), "the English catalog is empty");
			for (const std::string_view language : { "de", "fr", "ru" })
			{
				const auto translated = ShippedFields(language);
				for (const auto& [key, count] : english)
				{
					const auto found = translated.find(key);
					require(found != translated.end(), std::string{ language } + " lacks " + key);
					require(
						found->second == count,
						std::string{ language } + " changes the fields of " + key);
				}
				for (const auto& [key, count] : translated)
				{
					require(
						english.contains(key) || IsPluralVariant(english, key),
						std::string{ language } + " has a key English lacks: " + key);
				}
			}
		});

		runner.test("plural text follows each language's rules and reports stay English", [] {
			const auto render = [](uint64_t a_count, bool a_localized = true) {
				return Localization::Render(Support::Message::Plural(kErrorCount, a_count, a_count), a_localized);
			};
			{
				const LanguageScope language{ "ru" };
				require(render(1) == "1 \xD0\xBE\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB0", "ru one");
				require(render(21) == "21 \xD0\xBE\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB0", "ru 21 is one");
				require(render(3) == "3 \xD0\xBE\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB8", "ru few");
				require(render(12) == "12 \xD0\xBE\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBE\xD0\xBA", "ru 12 is many");
				require(render(3, false) == "3 errors", "reports stay English");
			}
			{
				const LanguageScope language{ "fr" };
				require(render(0) == "0 erreur", "French zero is singular");
			}
			{
				// No host file: the compiled English text must use English rules.
				const LanguageScope language{ "ja" };
				require(render(1) == "1 error", "ja without a file kept English singular");
			}
			{
				const LanguageScope language{ "ptbr" };
				require(render(0) == "0 errors", "ptbr without a file kept English plural");
			}
			{
				const LanguageScope language{ "en" };
				require(render(1) == "1 error" && render(2) == "2 errors", "English one and other");
			}
		});

		runner.test("client translations decode UTF-16 and fall back to English", [] {
			const TemporaryTranslation translated{
				"DmuiCatalogCheck_xx.txt",
				Utf16LittleEndian(kUtf16Greeting)
			};
			const TemporaryTranslation english{
				"DmuiCatalogCheck_en.txt",
				"$DMUI_Check_Greeting\tHello {}\n$DMUI_Check_EnglishOnly\tOnly English\n"
			};
			const LanguageScope language{ "xx" };
			const auto* greeting = Localization::FindTranslation("DmuiCatalogCheck", "$DMUI_Check_Greeting");
			require(
				greeting && *greeting == "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 {}",
				"UTF-16 LE text did not decode");
			const auto* fallback = Localization::FindTranslation("DmuiCatalogCheck", "$DMUI_Check_EnglishOnly");
			require(fallback && *fallback == "Only English", "a missing key did not fall back to English");
			require(
				Localization::FindTranslation("DmuiCatalogCheck", "$DMUI_Check_Missing") == nullptr,
				"an unknown key resolved");
		});

		runner.test("a malformed translated template shows English", [] {
			struct Broken final : Support::TextCatalog
			{
				[[nodiscard]] std::optional<std::string_view> Find(std::string_view) const noexcept override
				{
					return "{1} {:x} broken";
				}

				[[nodiscard]] Support::PluralCategory Plural(uint64_t) const noexcept override
				{
					return Support::PluralCategory::kOther;
				}
			};
			const Broken catalog;
			const Support::Message message{ Support::Phrase{ "$DMUI_Check_Template", "Saved {}." }, "file.toml" };
			require(message.Render(catalog) == "Saved file.toml.", "the English template was not used");
		});
	}
}
