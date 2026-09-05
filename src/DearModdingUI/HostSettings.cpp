#include <DearModdingUI/HostSettings.h>

#include <DearModdingUI/Host.h>
#include <DearModdingUI/Hotkeys.h>
#include <Support/Runtime.h>

#include <REX/REX.h>

#include <Windows.h>

#include <toml.hpp>

#include <atomic>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>

namespace DearModdingUI::HostSettings
{
	using namespace std::literals;

	namespace
	{
		std::atomic<bool> s_pageActive{ false };
		std::atomic<uint64_t> s_pageRevision{ 0 };
		std::mutex s_previewMutex;
		std::optional<HostInterfacePreviewSettings> s_preview;
		std::once_flag s_loadOnce;
		std::mutex s_settingsMutex;
		PersistedHostInterfaceSettings s_settings;
		std::atomic<uint32_t> s_menuToggleKey{ kMenuDefaultToggleKey };

		[[nodiscard]] std::filesystem::path ConfigPath()
		{
			return std::filesystem::path{ Addictol::Support::GetRuntimeDirectory() } /
				L"Data/F4SE/Plugins/DearModdingUI.toml";
		}

		[[nodiscard]] PersistedHostInterfaceSettings LoadSettings()
		{
			PersistedHostInterfaceSettings settings;
			const auto path = ConfigPath();
			if (!std::filesystem::exists(path))
				return settings;

			const auto root = toml::parse(path.string());
			const auto& section = toml::find(root, "Additional");
			settings.monochromeIcons = toml::find_or<bool>(
				section, "bMenuMonochromeIcons", settings.monochromeIcons);
			settings.sidebarLayout = toml::find_or<std::string>(
				section, "sMenuSidebarLayout", settings.sidebarLayout);
			settings.accentColor = toml::find_or<std::string>(
				section, "sMenuAccentColor", settings.accentColor);
			settings.windowBackgroundOpacity = toml::find_or<float>(
				section, "fMenuWindowOpacity", settings.windowBackgroundOpacity);
			settings.paletteBackgroundColor = toml::find_or<std::string>(
				section, "sMenuPaletteBackgroundColor", settings.paletteBackgroundColor);
			settings.paletteBackgroundOpacity = toml::find_or<float>(
				section, "fMenuPaletteOpacity", settings.paletteBackgroundOpacity);
			settings.backgroundBlur = toml::find_or<bool>(
				section, "bMenuBackgroundBlur", settings.backgroundBlur);
			settings.backgroundBlurStrength = toml::find_or<float>(
				section, "fMenuBackgroundBlurStrength", settings.backgroundBlurStrength);
			settings.uiScale = toml::find_or<float>(
				section, "fMenuUiScale", settings.uiScale);
			settings.bodyFontFamily = toml::find_or<std::string>(
				section, "sMenuBodyFontFamily", settings.bodyFontFamily);
			settings.menuToggleKey = toml::find_or<std::string>(
				section, "sMenuToggleKey", settings.menuToggleKey);
			if (root.contains("Hotkeys") && root.at("Hotkeys").is_table())
			{
				for (const auto& [id, value] : root.at("Hotkeys").as_table())
				{
					if (value.is_string())
						settings.hotkeys.emplace(id, value.as_string());
				}
			}
			const auto parsed = ParseMenuToggleKey(settings.menuToggleKey);
			if (!parsed.recognized)
			{
				REX::WARN(
					"DearModdingUI: sMenuToggleKey \"{}\" is not one of F1-F12, Home, End, Insert, or Delete; falling back to F11."sv,
					settings.menuToggleKey);
			}
			if (!ParseUserSidebarLayout(settings.sidebarLayout))
			{
				REX::WARN(
					"DearModdingUI: sMenuSidebarLayout \"{}\" is not an available user layout; falling back to \"{}\"."sv,
					settings.sidebarLayout,
					SidebarLayoutKindName(DEFAULT_SIDEBAR_LAYOUT));
			}
			return EncodeHostInterfaceSettings(DecodeHostInterfaceSettings(settings));
		}

		void EnsureLoaded() noexcept
		{
			std::call_once(s_loadOnce, []() noexcept {
				try
				{
					const std::scoped_lock lock{ s_settingsMutex };
					s_settings = LoadSettings();
					s_menuToggleKey.store(
						ParseMenuToggleKey(s_settings.menuToggleKey).virtualKey,
						std::memory_order_release);
					Hotkeys::InitializeOverrides(s_settings.hotkeys);
					Hotkeys::SetReservedVirtualKey(
						s_menuToggleKey.load(std::memory_order_acquire));
					REX::INFO("DearModdingUI: loaded host settings from {}"sv,
						ConfigPath().string());
					REX::INFO("DearModdingUI: menu toggle key {}"sv,
						s_settings.menuToggleKey);
				}
				catch (const std::exception& error)
				{
					REX::WARN("DearModdingUI: host settings could not be loaded: {}"sv,
						error.what());
				}
				catch (...)
				{
					REX::WARN("DearModdingUI: host settings could not be loaded"sv);
				}
			});
		}

		[[nodiscard]] bool SaveSettings(
			const PersistedHostInterfaceSettings& a_settings,
			std::string& a_error) noexcept
		{
			try
			{
				toml::value root{ toml::table{} };
				root["Additional"] = toml::table{};
				auto& section = root["Additional"];
				section["bMenuMonochromeIcons"] = a_settings.monochromeIcons;
				section["sMenuSidebarLayout"] = a_settings.sidebarLayout;
				section["sMenuAccentColor"] = a_settings.accentColor;
				section["fMenuWindowOpacity"] = static_cast<double>(
					a_settings.windowBackgroundOpacity);
				section["sMenuPaletteBackgroundColor"] =
					a_settings.paletteBackgroundColor;
				section["fMenuPaletteOpacity"] = static_cast<double>(
					a_settings.paletteBackgroundOpacity);
				section["bMenuBackgroundBlur"] = a_settings.backgroundBlur;
				section["fMenuBackgroundBlurStrength"] = static_cast<double>(
					a_settings.backgroundBlurStrength);
				section["fMenuUiScale"] = static_cast<double>(a_settings.uiScale);
				section["sMenuBodyFontFamily"] = a_settings.bodyFontFamily;
				section["sMenuToggleKey"] = a_settings.menuToggleKey;
				root["Hotkeys"] = toml::table{};
				for (const auto& [id, chord] : a_settings.hotkeys)
					root["Hotkeys"][id] = chord;

				const auto path = ConfigPath();
				std::filesystem::create_directories(path.parent_path());
				auto temporary = path;
				temporary += L".tmp";
				{
					std::ofstream output{
						temporary,
						std::ios::binary | std::ios::trunc
					};
					if (!output)
					{
						a_error = "DearModdingUI.toml could not be opened for writing.";
						return false;
					}
					output << toml::format(root);
					output.flush();
					if (!output)
					{
						a_error = "DearModdingUI.toml could not be written.";
						return false;
					}
				}

				if (!MoveFileExW(
						temporary.c_str(),
						path.c_str(),
						MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				{
					const auto error = GetLastError();
					std::error_code ignored;
					std::filesystem::remove(temporary, ignored);
					a_error = std::format(
						"DearModdingUI.toml could not be replaced (error {}).",
						error);
					return false;
				}
				return true;
			}
			catch (const std::exception& error)
			{
				a_error = error.what();
				return false;
			}
			catch (...)
			{
				a_error = "DearModdingUI.toml could not be persisted.";
				return false;
			}
		}

		void StorePageActive(bool a_active) noexcept
		{
			const auto current = s_pageActive.load(std::memory_order_acquire);
			const std::scoped_lock lock{ s_previewMutex };
			if (current && !a_active)
			{
				s_preview.reset();
				s_pageRevision.fetch_add(1, std::memory_order_release);
			}
			s_pageActive.store(a_active, std::memory_order_release);
		}
	}

	void Initialize() noexcept
	{
		EnsureLoaded();
	}

	HostInterfaceSettings Current() noexcept
	{
		EnsureLoaded();
		const std::scoped_lock lock{ s_settingsMutex };
		return DecodeHostInterfaceSettings(s_settings);
	}

	HostInterfacePreviewSettings EffectivePreview() noexcept
	{
		{
			const std::scoped_lock lock{ s_previewMutex };
			if (s_preview)
				return *s_preview;
		}
		return PreviewHostInterfaceSettings(Current());
	}

	bool Apply(HostInterfaceSettings a_settings) noexcept
	{
		EnsureLoaded();
		const auto persisted = EncodeHostInterfaceSettings(
			DecodeHostInterfaceSettings(EncodeHostInterfaceSettings(a_settings)));
		std::string error;
		{
			const std::scoped_lock lock{ s_settingsMutex };
			auto complete = persisted;
			complete.hotkeys = s_settings.hotkeys;
			if (complete == s_settings)
			{
				(void)SetHostStatus(
					DMUI_STATUS_SEVERITY_SUCCESS,
					"Settings saved.");
				return true;
			}
			if (!SaveSettings(complete, error))
			{
				REX::WARN(
					"DearModdingUI: interface settings could not be persisted: {}"sv,
					error);
				(void)SetHostStatus(DMUI_STATUS_SEVERITY_ERROR, error);
				return false;
			}
			s_settings = std::move(complete);
			s_menuToggleKey.store(
				ParseMenuToggleKey(s_settings.menuToggleKey).virtualKey,
				std::memory_order_release);
			Hotkeys::SetReservedVirtualKey(
				s_menuToggleKey.load(std::memory_order_acquire));
		}
		(void)SetHostStatus(
			DMUI_STATUS_SEVERITY_SUCCESS,
			"Settings saved.");
		return true;
	}

	bool SetSidebarLayout(SidebarLayoutKind a_layout) noexcept
	{
		EnsureLoaded();
		const auto layout = NormalizeUserSidebarLayout(a_layout);
		const auto persistedLayout = std::string{ SidebarLayoutKindName(layout) };
		std::string error;
		auto saved = true;
		{
			const std::scoped_lock lock{ s_settingsMutex };
			if (s_settings.sidebarLayout != persistedLayout)
			{
				auto updated = s_settings;
				updated.sidebarLayout = persistedLayout;
				if (SaveSettings(updated, error))
					s_settings = std::move(updated);
				else
					saved = false;
			}
		}
		if (!saved)
		{
			REX::WARN(
				"DearModdingUI: sidebar layout could not be persisted: {}"sv,
				error);
			(void)SetHostStatus(DMUI_STATUS_SEVERITY_ERROR, error);
			return false;
		}

		const std::scoped_lock lock{ s_previewMutex };
		if (s_preview)
			s_preview->sidebarLayout = layout;
		return true;
	}

	void SetPreview(
		HostInterfacePreviewSettings a_settings,
		uint64_t a_pageRevision) noexcept
	{
		const std::scoped_lock lock{ s_previewMutex };
		if (!s_pageActive.load(std::memory_order_acquire) ||
			s_pageRevision.load(std::memory_order_acquire) !=
				a_pageRevision)
			return;
		s_preview = a_settings;
	}

	void NotifyMenuVisible(bool a_visible) noexcept
	{
		if (!a_visible)
			StorePageActive(false);
	}

	void SetPageActive(bool a_active) noexcept
	{
		StorePageActive(a_active);
	}

	uint64_t PageRevision() noexcept
	{
		return s_pageRevision.load(std::memory_order_acquire);
	}

	uint32_t MenuToggleVirtualKey() noexcept
	{
		EnsureLoaded();
		return s_menuToggleKey.load(std::memory_order_acquire);
	}

	bool SetHotkeyOverride(
		std::string_view a_id,
		std::string_view a_chord) noexcept
	{
		EnsureLoaded();
		const auto previous = Hotkeys::Overrides();
		const auto result = Hotkeys::SetOverride(a_id, a_chord);
		if (result != DMUI_RESULT_OK)
		{
			Hotkeys::InitializeOverrides(previous);
			return false;
		}
		std::string error;
		{
			const std::scoped_lock lock{ s_settingsMutex };
			auto updated = s_settings;
			updated.hotkeys = Hotkeys::Overrides();
			if (SaveSettings(updated, error))
			{
				s_settings = std::move(updated);
				return true;
			}
		}
		Hotkeys::InitializeOverrides(previous);
		(void)SetHostStatus(DMUI_STATUS_SEVERITY_ERROR, error);
		return false;
	}

	bool RemoveHotkeyOverride(std::string_view a_id) noexcept
	{
		EnsureLoaded();
		const auto previous = Hotkeys::Overrides();
		if (!Hotkeys::RemoveOverride(a_id))
			return false;
		std::string error;
		{
			const std::scoped_lock lock{ s_settingsMutex };
			auto updated = s_settings;
			updated.hotkeys = Hotkeys::Overrides();
			if (SaveSettings(updated, error))
			{
				s_settings = std::move(updated);
				return true;
			}
		}
		Hotkeys::InitializeOverrides(previous);
		(void)SetHostStatus(DMUI_STATUS_SEVERITY_ERROR, error);
		return false;
	}
}
