#include <DearModdingUI/HostSettingsHealth.h>

#include <toml.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <format>
#include <system_error>
#include <utility>

namespace DearModdingUI
{
	namespace
	{
		template <class T>
		[[nodiscard]] T ReadSetting(
			const toml::value& a_section,
			std::string_view a_key,
			T a_fallback,
			std::vector<std::string>& a_corrections)
		{
			const std::string key{ a_key };
			if (!a_section.contains(key))
				return a_fallback;
			try
			{
				return toml::find<T>(a_section, key);
			}
			catch (const std::exception&)
			{
				a_corrections.push_back(
					std::format("{} had the wrong value type and used its default",
						a_key));
				return a_fallback;
			}
		}

		void AppendCorrection(
			std::vector<std::string>& a_corrections,
			std::string a_correction)
		{
			a_corrections.push_back(std::move(a_correction));
		}

		[[nodiscard]] PersistedHostInterfaceSettings NormalizeSettings(
			PersistedHostInterfaceSettings a_settings,
			std::vector<std::string>& a_corrections)
		{
			const auto runtime = DecodeHostInterfaceSettings(a_settings);
			auto normalized = EncodeHostInterfaceSettings(runtime);
			normalized.hotkeys = std::move(a_settings.hotkeys);

			if (!ParseUserSidebarLayout(a_settings.sidebarLayout))
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuSidebarLayout \"{}\" used \"{}\"",
						a_settings.sidebarLayout,
						normalized.sidebarLayout));
			}
			if (!TryDecodeHostColor(a_settings.accentColor))
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuAccentColor \"{}\" used {}",
						a_settings.accentColor,
						normalized.accentColor));
			}
			if (runtime.windowBackgroundOpacity !=
				a_settings.windowBackgroundOpacity)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"fMenuWindowOpacity {} used {}",
						a_settings.windowBackgroundOpacity,
						runtime.windowBackgroundOpacity));
			}
			if (!TryDecodeHostColor(a_settings.paletteBackgroundColor))
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuPaletteBackgroundColor \"{}\" used {}",
						a_settings.paletteBackgroundColor,
						normalized.paletteBackgroundColor));
			}
			if (runtime.paletteBackgroundOpacity !=
				a_settings.paletteBackgroundOpacity)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"fMenuPaletteOpacity {} used {}",
						a_settings.paletteBackgroundOpacity,
						runtime.paletteBackgroundOpacity));
			}
			if (runtime.backgroundBlurStrength !=
				a_settings.backgroundBlurStrength)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"fMenuBackgroundBlurStrength {} used {}",
						a_settings.backgroundBlurStrength,
						runtime.backgroundBlurStrength));
			}
			if (runtime.uiScale != a_settings.uiScale)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"fMenuUiScale {} used {}",
						a_settings.uiScale,
						runtime.uiScale));
			}
			if (!IsValidBodyFontFamily(a_settings.bodyFontFamily))
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuBodyFontFamily \"{}\" used \"{}\"",
						a_settings.bodyFontFamily,
						normalized.bodyFontFamily));
			}
			if (!ParseMenuToggleKey(a_settings.menuToggleKey).recognized)
			{
				AppendCorrection(
					a_corrections,
					std::format(
						"sMenuToggleKey \"{}\" used \"{}\"",
						a_settings.menuToggleKey,
						normalized.menuToggleKey));
			}
			return normalized;
		}

		[[nodiscard]] std::string CorrectionSummary(
			const std::vector<std::string>& a_corrections)
		{
			constexpr size_t kVisibleCorrectionLimit = 5;
			std::string result{ "Using corrected settings: " };
			const auto visible = (std::min)(
				a_corrections.size(),
				kVisibleCorrectionLimit);
			for (size_t index = 0; index < visible; ++index)
			{
				if (index != 0)
					result.append("; ");
				result.append(a_corrections[index]);
			}
			if (a_corrections.size() > visible)
			{
				result.append(std::format(
					"; and {} more correction{}",
					a_corrections.size() - visible,
					a_corrections.size() - visible == 1 ? "" : "s"));
			}
			result.append(". Save Settings to persist the accepted values.");
			return result;
		}
	}

	HostSettingsLoadResult LoadHostInterfaceSettings(
		const std::filesystem::path& a_path)
	{
		HostSettingsLoadResult result;
		result.path = a_path.filename().string();
		std::error_code existsError;
		const auto exists = std::filesystem::exists(a_path, existsError);
		if (existsError)
		{
			result.disposition = HostSettingsLoadDisposition::kFailed;
			result.detail = std::format(
				"Could not inspect {}: {}. Using defaults; check file permissions and restart.",
				result.path,
				existsError.message());
			return result;
		}
		if (!exists)
		{
			result.disposition = HostSettingsLoadDisposition::kMissing;
			result.detail = "Using defaults; configuration file is absent.";
			return result;
		}

		try
		{
			const auto root = toml::parse(a_path.string());
			const auto& section = toml::find(root, "Additional");
			auto& settings = result.settings;
			settings.monochromeIcons = ReadSetting<bool>(
				section,
				"bMenuMonochromeIcons",
				settings.monochromeIcons,
				result.corrections);
			settings.sidebarLayout = ReadSetting<std::string>(
				section,
				"sMenuSidebarLayout",
				settings.sidebarLayout,
				result.corrections);
			settings.accentColor = ReadSetting<std::string>(
				section,
				"sMenuAccentColor",
				settings.accentColor,
				result.corrections);
			settings.windowBackgroundOpacity = ReadSetting<float>(
				section,
				"fMenuWindowOpacity",
				settings.windowBackgroundOpacity,
				result.corrections);
			settings.paletteBackgroundColor = ReadSetting<std::string>(
				section,
				"sMenuPaletteBackgroundColor",
				settings.paletteBackgroundColor,
				result.corrections);
			settings.paletteBackgroundOpacity = ReadSetting<float>(
				section,
				"fMenuPaletteOpacity",
				settings.paletteBackgroundOpacity,
				result.corrections);
			settings.backgroundBlur = ReadSetting<bool>(
				section,
				"bMenuBackgroundBlur",
				settings.backgroundBlur,
				result.corrections);
			settings.backgroundBlurStrength = ReadSetting<float>(
				section,
				"fMenuBackgroundBlurStrength",
				settings.backgroundBlurStrength,
				result.corrections);
			settings.uiScale = ReadSetting<float>(
				section,
				"fMenuUiScale",
				settings.uiScale,
				result.corrections);
			settings.bodyFontFamily = ReadSetting<std::string>(
				section,
				"sMenuBodyFontFamily",
				settings.bodyFontFamily,
				result.corrections);
			settings.menuToggleKey = ReadSetting<std::string>(
				section,
				"sMenuToggleKey",
				settings.menuToggleKey,
				result.corrections);

			if (root.contains("Hotkeys") && root.at("Hotkeys").is_table())
			{
				for (const auto& [id, value] : root.at("Hotkeys").as_table())
				{
					if (value.is_string())
						settings.hotkeys.emplace(id, value.as_string());
					else
						result.corrections.push_back(
							std::format("Hotkeys.{} was ignored because it was not text",
								id));
				}
			}
			result.settings = NormalizeSettings(
				std::move(settings),
				result.corrections);
			result.disposition = result.corrections.empty() ?
				HostSettingsLoadDisposition::kLoaded :
				HostSettingsLoadDisposition::kCorrected;
			result.detail = result.corrections.empty() ?
				std::format("Loaded settings from {}.", result.path) :
				CorrectionSummary(result.corrections);
		}
		catch (const std::exception& error)
		{
			result.settings = {};
			result.disposition = HostSettingsLoadDisposition::kFailed;
			result.detail = std::format(
				"Could not load {}: {}. Using defaults; correct or remove the file and restart.",
				result.path,
				error.what());
		}
		catch (...)
		{
			result.settings = {};
			result.disposition = HostSettingsLoadDisposition::kFailed;
			result.detail = std::format(
				"Could not load {}. Using defaults; correct or remove the file and restart.",
				result.path);
		}
		return result;
	}

	void HostSettingsHealthState::RecordLoad(HostSettingsLoadResult a_result)
	{
		load_ = std::move(a_result);
	}

	void HostSettingsHealthState::RecordSaveFailure(std::string_view a_error)
	{
		saveError_ = a_error;
	}

	void HostSettingsHealthState::RecordSaveSuccess(std::string_view a_path)
	{
		saveError_.reset();
		load_.disposition = HostSettingsLoadDisposition::kLoaded;
		load_.path = a_path;
		load_.detail = std::format("Saved accepted settings to {}.", a_path);
		load_.corrections.clear();
	}

	HealthObservation HostSettingsHealthState::Observation() const
	{
		HealthObservation observation;
		observation.state =
			load_.disposition == HostSettingsLoadDisposition::kMissing ||
				load_.disposition == HostSettingsLoadDisposition::kLoaded ?
			HealthState::kReady :
			HealthState::kDegraded;
		observation.reason = load_.detail;
		if (saveError_)
		{
			observation.state = HealthState::kDegraded;
			if (!observation.reason.empty())
				observation.reason.push_back(' ');
			observation.reason.append(
				"Settings remain active, but the latest save failed: ");
			observation.reason.append(*saveError_);
			observation.reason.append(
				" Check write permissions and available disk space, then save again.");
		}
		return observation;
	}
}
