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

	[[nodiscard]] SourceValueKind ResolveSourceValueKind(
		SourceValueKind a_declared,
		const dmui::SettingValue& a_default) noexcept;

	[[nodiscard]] std::optional<dmui::SettingValue> GlobalToSettingValue(
		float a_value,
		SourceValueKind a_kind) noexcept;

	[[nodiscard]] std::optional<float> SettingValueToGlobal(
		const dmui::SettingValue& a_value,
		SourceValueKind a_kind) noexcept;
}
