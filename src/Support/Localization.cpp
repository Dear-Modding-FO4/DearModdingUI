#include <Support/Localization.h>
#include <Support/Runtime.h>

#include <RE/S/Setting.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_map>

namespace DearModdingUI::Support
{
	using namespace std::literals;
	using namespace Addictol::Support;

	// Same as MCM translation files.
	// String parsing: $Weather40	Сильная облачность с дождем
	class LocalizationFileLoader
	{
		std::unordered_map<std::string, std::string> translations{};

		bool UTF8_LoadLanguageFile(std::ifstream& a_stm, const std::string& a_filePath,
			uintmax_t a_size, bool a_isBom)
		{
			try
			{
				if (a_isBom)
					a_stm.seekg(3, std::ios::beg);

				std::string line;
				while (std::getline(a_stm, line))
				{
					Trim(line);

					std::size_t delimiterPos = line.find_first_of(" \t");
					if (delimiterPos != std::string::npos)
					{
						std::string key = line.substr(0, delimiterPos);
						std::string value = line.substr(delimiterPos + 1);

						Trim(key);
						Trim(value);

						translations.try_emplace(key, value);
					}
				}

				return true;
			}
			catch (const std::exception& e)
			{
				REX::ERROR("An exception occurred while reading the file: \"{}\" message: \"{}\" "sv,
					a_filePath, e.what());
				return false;
			}
		}

		bool UTF16LE_LoadLanguageFile(std::ifstream& a_stm, const std::string& a_filePath, 
			uintmax_t a_size, bool a_isBom)
		{
			try
			{
				if (a_isBom)
					a_stm.seekg(2, std::ios::beg);

				std::string u16;
				u16.resize(a_size);
				a_stm.read(u16.data(), a_size - 2);

				std::string u8, line;
				REX::UTF16_TO_UTF8(reinterpret_cast<const wchar_t*>(u16.c_str()), u8);

				std::stringstream sstm(u8);

				while (std::getline(sstm, line))
				{
					Trim(line);

					std::size_t delimiterPos = line.find_first_of(" \t");
					if (delimiterPos != std::string::npos)
					{
						std::string key = line.substr(0, delimiterPos);
						std::string value = line.substr(delimiterPos + 1);

						Trim(key);
						Trim(value);

						translations.try_emplace(key, value);
					}
				}

				return true;
			}
			catch (const std::exception& e)
			{
				REX::ERROR("An exception occurred while reading the file: \"{}\" message: \"{}\" "sv,
					a_filePath, e.what());
				return false;
			}
		}

		bool UTF16BE_LoadLanguageFile(std::ifstream& a_stm, const std::string& a_filePath,
			uintmax_t a_size, bool a_isBom)
		{
			try
			{
				if (a_isBom)
					a_stm.seekg(2, std::ios::beg);

				std::string u16;
				u16.resize(a_size);
				a_stm.read(u16.data(), a_size - 2);

				auto swap_bytes = [&](char* data, size_t byte_length) {
					for (size_t i = 0; i + 1 < byte_length; i += 2)
						std::swap(data[i], data[i + 1]);
					};

				swap_bytes(u16.data(), u16.size());

				std::string u8, line;
				REX::UTF16_TO_UTF8(reinterpret_cast<const wchar_t*>(u16.c_str()), u8);

				std::stringstream sstm(u8);

				while (std::getline(sstm, line))
				{
					Trim(line);

					std::size_t delimiterPos = line.find_first_of(" \t");
					if (delimiterPos != std::string::npos)
					{
						std::string key = line.substr(0, delimiterPos);
						std::string value = line.substr(delimiterPos + 1);

						Trim(key);
						Trim(value);

						translations.try_emplace(key, value);
					}
				}

				return true;
			}
			catch (const std::exception& e)
			{
				REX::ERROR("An exception occurred while reading the file: \"{}\" message: \"{}\" "sv,
					a_filePath, e.what());
				return false;
			}
		}
	public:
		constexpr LocalizationFileLoader() = default;

		// Reads and parses the localization file line-by-line
		bool LoadLanguageFile(const std::string& a_filePath)
		{
			if (!std::filesystem::exists(a_filePath))
			{
				REX::WARN("No found localization file: {}"sv, a_filePath);
				return false;
			}

			auto fileSize = std::filesystem::file_size(a_filePath);
			if (fileSize <= 4)
			{
				REX::WARN("Incorrect file, too small size: {}"sv, a_filePath);
				return false;
			}

			auto encoding = CheckBom(a_filePath);
			if ((encoding == Encoding::UTF32_BE) || (encoding == Encoding::UTF32_LE))
			{
				REX::ERROR("The file contains a bom and the file encoding is not supported: {}"sv, a_filePath);
				return false;
			}

			bool isBom = encoding != Encoding::Unknown;

			std::ifstream file(a_filePath, std::ios::binary);
			if (!file.is_open())
			{
				REX::WARN("Failed to open localization file: {}"sv, a_filePath);
				return false;
			}

			switch (encoding)
			{
			case Encoding::UTF16_LE:
				return UTF16LE_LoadLanguageFile(file, a_filePath, fileSize, isBom);
			case Encoding::UTF16_BE:
				return UTF16BE_LoadLanguageFile(file, a_filePath, fileSize, isBom);
			default:
				return UTF8_LoadLanguageFile(file, a_filePath, fileSize, isBom);
			}

			file.close();
			return true;
		}

		// Fetches the localized text
		bool Get(const std::string& a_key, const std::string& a_defValue, std::string& a_value) const noexcept
		{
			auto it = translations.find(a_key);
			auto result = it != translations.end();
			if (result) a_value = it->second;
			else a_value = a_defValue;
			return result;
		}
	};

	BaseLocalizeString::BaseLocalizeString(const std::string& a_default) noexcept :
		value(a_default),
		valueDefault(a_default)
	{}

	std::string BaseLocalizeString::GetValue() const noexcept { return value; }
	std::string BaseLocalizeString::GetValueDefault() const noexcept { return valueDefault; }
	void BaseLocalizeString::SetValue(const std::string& a_value) noexcept { value = a_value; }

	BaseLocalizeString::operator std::string& () noexcept { return value; }
	BaseLocalizeString::operator const std::string& () const noexcept { return value; }

	void LocalizeStore::Init(const std::string& a_file, bool a_isMultilang) noexcept
	{ 
		file = a_file;
		if (a_isMultilang)
		{
			// Retrieve the global collection of INI settings
			auto settings = RE::INISettingCollection::GetSingleton();
			if (!settings)
			{
				REX::WARN("RE::INISettingCollection::GetSingleton return nullptr");
				return;
			}

			// Look up the SLanguage:General setting
			// Yeah, exactly SLanguage:General this Bethesda
			auto setting = settings->GetSetting("SLanguage:General");

			// dump
			/*for (auto& s : settings->settings)
			{
				REX::INFO(s->GetKey());
			}*/

			if (setting && (setting->GetType() == RE::Setting::SETTING_TYPE::kString))
			{
				std::string lang = setting->GetString().data();
				lang.insert(0, "_");

				auto it = a_file.find_last_of('.');
				if (it == std::string::npos)
					file += lang.data();
				else
					file.insert(it, lang.data());
			}
			else
			{
				REX::WARN("RE::INISettingCollection::GetSetting no found \"sLanguage:General\" setting");
				return;
			}
		}
	}

	bool LocalizeStore::Exists() const noexcept
	{
		return std::filesystem::exists(file);
	}

	void LocalizeStore::Add(ILocalizeString* a_localize) noexcept
	{
		if (a_localize)
			localizes.emplace_back(a_localize);
	}

	std::string LocalizeStore::GetFileName() const noexcept
	{
		return file;
	}

	void LocalizationManager::Load()
	{
		LocalizationFileLoader loader;

		loader.LoadLanguageFile(file);
		for (auto& localize : localizes)
			localize->Load(std::addressof(loader));
	}

	namespace Impl
	{
		static void LocalizeLoad(void* a_data, const std::string& a_key, 
			std::string& a_value, const std::string& a_valueDefault) noexcept
		{
			const auto data = static_cast<LocalizationFileLoader*>(a_data);
			data->Get(a_key, a_valueDefault, a_value);
		}
	}

	LocalizeString::LocalizeString(const std::string& a_key, const std::string& a_default) noexcept :
		BaseLocalizeString(a_default),
		key(a_key)
	{
		LocalizationManager::GetSingleton()->Add(this);
	}

	void LocalizeString::Load(void* a_data) noexcept
	{
		Impl::LocalizeLoad(a_data, key, this->value, this->valueDefault);
	}
}
