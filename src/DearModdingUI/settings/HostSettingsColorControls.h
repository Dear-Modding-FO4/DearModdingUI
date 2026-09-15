#pragma once

#include <DearModdingUI/settings/HostSettings.h>

#include <cstddef>
#include <optional>
#include <span>

namespace DearModdingUI::HostSettingsViewDetail
{
	struct ColorPreset
	{
		const char* name;
		const char* description;
		HostAccentColor color;
	};

	struct ColorSettingControlResult
	{
		bool changed{};
		bool presetsInline{};
		float pickerMinX{};
		float pickerMaxX{};
		float pickerHeight{};
		float presetsMinX{};
		float presetsMaxX{};
		float presetHeight{};
	};

	[[nodiscard]] ColorSettingControlResult DrawColorSettingControl(
		HostAccentColor& a_color,
		std::span<const ColorPreset> a_presets,
		float a_width) noexcept;

	[[nodiscard]] bool DrawGameColorSyncControls(
		HostAccentColor& a_color,
		std::optional<HostAccentColor> a_hudColor,
		std::optional<HostAccentColor> a_pipboyColor,
		float a_width) noexcept;
}
