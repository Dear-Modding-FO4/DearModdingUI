#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace DearModdingUI::MCM
{
	struct GlobalFormReference
	{
		std::string plugin;
		uint32_t localId{};
	};

	[[nodiscard]] std::optional<GlobalFormReference> ParseGlobalFormReference(
		std::string_view a_sourceForm) noexcept;

	[[nodiscard]] std::optional<dmui::SettingValue> GlobalToSettingValue(
		float a_value,
		const dmui::SettingValue& a_target) noexcept;

	[[nodiscard]] std::optional<float> SettingValueToGlobal(
		const dmui::SettingValue& a_value) noexcept;
}
