#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI::MCM
{
	struct SettingIdentifier
	{
		std::string key;
		std::string section;

		[[nodiscard]] bool operator==(
			const SettingIdentifier&) const noexcept = default;
	};

	struct SettingsIni
	{
		bool available{};
		std::vector<SettingIdentifier> declarations;

		[[nodiscard]] bool Contains(
			const SettingIdentifier& a_setting) const noexcept;
	};

	[[nodiscard]] std::optional<SettingIdentifier> ParseSettingIdentifier(
		std::string_view a_id) noexcept;

	[[nodiscard]] SettingsIni ParseSettingsIni(
		std::string_view a_ini) noexcept;

	[[nodiscard]] SettingsIni LoadSettingsIni(
		const std::filesystem::path& a_path) noexcept;

	void ApplyDeclarations(
		MappedPage& a_page,
		const SettingsIni& a_settings) noexcept;
}
