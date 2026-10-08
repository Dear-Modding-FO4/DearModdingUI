#include <Platform/localization/TranslationFiles.h>

#ifndef DMUI_PREVIEW
#include <RE/S/Setting.h>

#include <algorithm>
#include <cctype>
#endif

#include <Windows.h>

#include <fstream>
#include <iterator>

namespace DearModdingUI
{
	std::string ReadGameLanguage() noexcept
	{
#ifdef DMUI_PREVIEW
		return {};
#else
		try
		{
			const auto* setting = RE::GetINISetting("sLanguage:General");
			if (!setting || setting->GetType() != RE::Setting::SETTING_TYPE::kString)
				return {};
			std::string language{ setting->GetString() };
			std::ranges::transform(language, language.begin(), [](unsigned char a_character) {
				return static_cast<char>(std::tolower(a_character));
			});
			return language;
		}
		catch (...)
		{
			return {};
		}
#endif
	}

	std::optional<std::string> ReadTranslationFile(const std::filesystem::path& a_path) noexcept
	{
		try
		{
			std::ifstream file{ a_path, std::ios::binary };
			if (!file)
				return std::nullopt;
			std::string bytes{ std::istreambuf_iterator<char>{ file }, {} };
			if (bytes.starts_with("\xEF\xBB\xBF"))
				return bytes.substr(3);
			if (!bytes.starts_with("\xFF\xFE"))
				return bytes;
			const auto units = static_cast<int>((bytes.size() - 2) / sizeof(wchar_t));
			const auto* wide = reinterpret_cast<const wchar_t*>(bytes.data() + 2);
			const auto size = ::WideCharToMultiByte(CP_UTF8, 0, wide, units, nullptr, 0, nullptr, nullptr);
			std::string text(static_cast<size_t>(size), '\0');
			if (size > 0)
				::WideCharToMultiByte(CP_UTF8, 0, wide, units, text.data(), size, nullptr, nullptr);
			return text;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}
}
