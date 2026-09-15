#include <Platform/settings/GameColors.h>

#ifndef DMUI_PREVIEW
#include <RE/S/Setting.h>

#include <array>
#include <cmath>
#include <string_view>
#endif

namespace DearModdingUI
{
	std::optional<HostAccentColor> ReadGameColor(
		GameColorSource a_source) noexcept
	{
#ifdef DMUI_PREVIEW
		(void)a_source;
		return std::nullopt;
#else
		auto* preferences = RE::INIPrefSettingCollection::GetSingleton();
		if (!preferences)
			return std::nullopt;

		const auto hud = a_source == GameColorSource::kHUD;
		const std::array<std::string_view, 3> names = hud ?
			std::array<std::string_view, 3>{
				"iHUDColorR:Interface",
				"iHUDColorG:Interface",
				"iHUDColorB:Interface"
			} :
			std::array<std::string_view, 3>{
				"fPipboyEffectColorR:Pipboy",
				"fPipboyEffectColorG:Pipboy",
				"fPipboyEffectColorB:Pipboy"
			};
		const auto type = hud ?
			RE::Setting::SETTING_TYPE::kInt :
			RE::Setting::SETTING_TYPE::kFloat;
		std::array<float, 3> channels{};
		for (size_t index = 0; index < names.size(); ++index)
		{
			const auto* setting = preferences->GetSetting(names[index]);
			if (!setting || setting->GetType() != type)
				return std::nullopt;
			const auto value = hud ?
				static_cast<float>(setting->GetInt()) / 255.0f :
				setting->GetFloat();
			if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
				return std::nullopt;
			channels[index] = value;
		}
		return HostAccentFromImVec4({
			channels[0], channels[1], channels[2], 1.0f
		});
#endif
	}
}
