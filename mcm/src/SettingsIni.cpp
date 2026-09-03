#include <DearModdingUI/MCM/SettingsIni.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace DearModdingUI::MCM
{
	namespace
	{
		constexpr std::string_view kMainSection = "Main";

		[[nodiscard]] std::string_view Trim(std::string_view a_value) noexcept
		{
			const auto whitespace = [](char a_character) {
				return a_character == ' ' || a_character == '\t' ||
					a_character == '\r' || a_character == '\n';
			};
			while (!a_value.empty() && whitespace(a_value.front()))
				a_value.remove_prefix(1);
			while (!a_value.empty() && whitespace(a_value.back()))
				a_value.remove_suffix(1);
			return a_value;
		}
	}

	bool SettingsIni::Contains(
		const SettingIdentifier& a_setting) const noexcept
	{
		return std::ranges::find(declarations, a_setting) != declarations.end();
	}

	std::optional<SettingIdentifier> ParseSettingIdentifier(
		std::string_view a_id) noexcept
	{
		try
		{
			const auto separator = a_id.find(':');
			if (separator == std::string_view::npos)
			{
				if (a_id.empty())
					return std::nullopt;
				return SettingIdentifier{
					std::string{ a_id },
					std::string{ kMainSection }
				};
			}
			if (separator == 0 ||
				separator + 1 == a_id.size() ||
				a_id.find(':', separator + 1) != std::string_view::npos)
				return std::nullopt;
			return SettingIdentifier{
				std::string{ a_id.substr(0, separator) },
				std::string{ a_id.substr(separator + 1) }
			};
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	SettingsIni ParseSettingsIni(std::string_view a_ini) noexcept
	{
		SettingsIni result;
		result.available = true;
		try
		{
			auto section = std::string{ kMainSection };
			for (size_t offset = 0; offset <= a_ini.size();)
			{
				const auto end = a_ini.find('\n', offset);
				auto line = Trim(a_ini.substr(
					offset,
					end == std::string_view::npos ?
						a_ini.size() - offset :
						end - offset));
				offset = end == std::string_view::npos ?
					a_ini.size() + 1 :
					end + 1;
				if (line.empty() || line.front() == ';' || line.front() == '#')
					continue;
				if (line.front() == '[' && line.back() == ']')
				{
					auto declared = Trim(line.substr(1, line.size() - 2));
					section = std::string{ declared };
					continue;
				}
				if (section.empty())
					continue;
				const auto separator = line.find('=');
				if (separator == std::string_view::npos)
					continue;
				const auto key = Trim(line.substr(0, separator));
				if (key.empty())
					continue;
				SettingIdentifier declaration{
					std::string{ key },
					section
				};
				if (!result.Contains(declaration))
					result.declarations.push_back(std::move(declaration));
			}
		}
		catch (...)
		{
			result.declarations.clear();
		}
		return result;
	}

	SettingsIni LoadSettingsIni(const std::filesystem::path& a_path) noexcept
	{
		try
		{
			std::ifstream stream{ a_path, std::ios::binary };
			if (!stream)
				return {};
			std::ostringstream buffer;
			buffer << stream.rdbuf();
			if (stream.bad())
				return {};
			return ParseSettingsIni(buffer.str());
		}
		catch (...)
		{
			return {};
		}
	}

	void ApplyDeclarations(
		MappedPage& a_page,
		const SettingsIni& a_settings) noexcept
	{
		for (auto& row : a_page.rows)
		{
			if (!row.binding)
				continue;
			auto* setting =
				std::get_if<ModSettingBinding>(&row.binding->source);
			if (!setting)
				continue;
			if (!a_settings.available)
			{
				setting->declaration = DeclarationState::kUnknown;
				continue;
			}
			setting->declaration = a_settings.Contains({
				setting->key,
				setting->section
			}) ?
				DeclarationState::kDeclared :
				DeclarationState::kUndeclared;
		}
	}
}
