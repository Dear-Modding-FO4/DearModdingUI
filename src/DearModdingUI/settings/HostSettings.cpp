#include <DearModdingUI/settings/HostSettings.h>

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/settings/HostSettingsHealthState.h>
#include <DearModdingUI/settings/HostSettingsPersistence.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/localization/Localization.h>
#include <Support/Revision.h>
#include <Support/Runtime.h>
#include <Support/SubsystemHealth.h>

#include <REX/REX.h>

#include <array>
#include <atomic>
#include <exception>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace DearModdingUI::HostSettings
{
	using namespace std::literals;

	namespace
	{
		constexpr Support::Phrase kSettingsSavedText{ "$DMUI_Settings_SettingsSaved", "Settings saved." };
		constexpr Support::Phrase kEstablishFailed{
			"$DMUI_Config_EstablishFailed",
			"Could not establish host settings: {}. Using defaults; restart after correcting the configuration."
		};
		constexpr Support::Phrase kEstablishFailedUnknown{
			"$DMUI_Config_EstablishFailedUnknown",
			"Could not establish host settings. Using defaults; restart after correcting the configuration."
		};

		std::atomic<bool> s_pageActive{ false };
		std::atomic<uint64_t> s_pageRevision{ 0 };
		std::mutex s_previewMutex;
		std::optional<HostInterfacePreviewSettings> s_preview;
		std::once_flag s_loadOnce;
		std::mutex s_settingsMutex;
		HostInterfaceSettings s_settings;
		using OverridesBySlot = std::array<std::map<std::string, std::string>, 2>;
		OverridesBySlot s_hotkeyOverrides;
		std::mutex s_configurationHealthMutex;
		HostSettingsHealthState s_configurationHealthState;
		// Covers both Current() and EffectivePreview().
		Support::Revision s_revision;

		class ConfigurationHealthReporter final : public HealthReporter
		{
		public:
			void Report(
				HealthEvent a_event,
				const HealthSnapshot& a_snapshot) noexcept override
			{
				if (a_event == HealthEvent::kRecovery)
				{
					REX::INFO("[{}] Configuration recovered: {}"sv,
						a_snapshot.identity,
						a_snapshot.reason.English());
					return;
				}
				if (a_snapshot.state == HealthState::kDegraded)
				{
					REX::WARN("[{}] Configuration {}: {}"sv,
						a_snapshot.identity,
						HealthStateLabel(a_snapshot.state),
						a_snapshot.reason.English());
				}
				else
				{
					REX::INFO("[{}] Configuration {}: {}"sv,
						a_snapshot.identity,
						HealthStateLabel(a_snapshot.state),
						a_snapshot.reason.English());
				}
			}
		};

		ConfigurationHealthReporter s_configurationHealthReporter;
		SubsystemHealth s_configurationHealth{
			"dmui.configuration",
			{ "$DMUI_Health_SubsystemConfiguration", "Configuration" },
			s_configurationHealthReporter,
			HostSubsystemHealthRegistry()
		};

		[[nodiscard]] std::filesystem::path ConfigPath()
		{
			return std::filesystem::path{ Support::GetRuntimeDirectory() } /
				L"Data/F4SE/Plugins/DearModdingUI.toml";
		}

		void PublishConfigurationHealth() noexcept
		{
			try
			{
				HealthObservation observation;
				{
					const std::scoped_lock lock{ s_configurationHealthMutex };
					observation = s_configurationHealthState.Observation();
				}
				(void)s_configurationHealth.Observe(
					observation.state,
					observation.reason);
			}
			catch (const std::exception& error)
			{
				REX::WARN(
					"DearModdingUI: configuration health could not be published: {}"sv,
					error.what());
			}
			catch (...)
			{
				REX::WARN(
					"DearModdingUI: configuration health could not be published"sv);
			}
		}

		void RecordSaveFailure(const Support::Message& a_error) noexcept
		{
			try
			{
				{
					const std::scoped_lock lock{ s_configurationHealthMutex };
					s_configurationHealthState.RecordSaveFailure(a_error);
				}
				PublishConfigurationHealth();
			}
			catch (...)
			{
				REX::WARN("DearModdingUI: save failure health could not be retained"sv);
			}
		}

		void RecordSaveSuccess() noexcept
		{
			try
			{
				{
					const std::scoped_lock lock{ s_configurationHealthMutex };
					s_configurationHealthState.RecordSaveSuccess(
						ConfigPath().filename().string());
				}
				PublishConfigurationHealth();
			}
			catch (...)
			{
				REX::WARN("DearModdingUI: save recovery health could not be retained"sv);
			}
		}

		void UpdateMenuToggleChord() noexcept
		{
			const auto chord = ParseMenuToggleChord(s_settings.menuToggleKey).chord;
			Hotkeys::SetReservedChord(chord);
			Hotkeys::SetReservedChord(
				ParseMenuToggleChord(s_settings.menuToggleGamepad, HotkeySlot::kGamepad).chord,
				HotkeySlot::kGamepad);
		}

		void EnsureLoaded() noexcept
		{
			std::call_once(s_loadOnce, []() noexcept {
				try
				{
					auto loaded = LoadHostInterfaceSettings(ConfigPath());
					const std::scoped_lock lock{ s_settingsMutex };
					s_settings = std::move(loaded.settings);
					s_hotkeyOverrides = { std::move(loaded.hotkeys), std::move(loaded.gamepadHotkeys) };
					for (const auto slot : kHotkeySlots)
						Hotkeys::InitializeOverrides(s_hotkeyOverrides[static_cast<size_t>(slot)], slot);
					UpdateMenuToggleChord();
					s_revision.Advance();
					{
						const std::scoped_lock healthLock{
							s_configurationHealthMutex
						};
						s_configurationHealthState.RecordLoad(
							loaded);
					}
					PublishConfigurationHealth();
					REX::INFO("DearModdingUI: menu toggle key {}"sv,
						s_settings.menuToggleKey);
				}
				catch (const std::exception& error)
				{
					REX::WARN("DearModdingUI: host settings could not be loaded: {}"sv,
						error.what());
					try
					{
						HostSettingsLoadResult failed;
						failed.disposition =
							HostSettingsLoadDisposition::kFailed;
						failed.path = ConfigPath().filename().string();
						failed.detail = Support::Message{ kEstablishFailed, error.what() };
						{
							const std::scoped_lock lock{
								s_configurationHealthMutex
							};
							s_configurationHealthState.RecordLoad(
								failed);
						}
						PublishConfigurationHealth();
					}
					catch (...)
					{}
				}
				catch (...)
				{
					REX::WARN("DearModdingUI: host settings could not be loaded"sv);
					try
					{
						HostSettingsLoadResult failed;
						failed.disposition =
							HostSettingsLoadDisposition::kFailed;
						failed.path = ConfigPath().filename().string();
						failed.detail = Support::Message{ kEstablishFailedUnknown };
						{
							const std::scoped_lock lock{
								s_configurationHealthMutex
							};
							s_configurationHealthState.RecordLoad(
								failed);
						}
						PublishConfigurationHealth();
					}
					catch (...)
					{}
				}
			});
		}

		[[nodiscard]] bool SaveSettings(
			const HostInterfaceSettings& a_settings,
			const OverridesBySlot& a_hotkeyOverrides,
			Support::Message& a_error) noexcept
		{
			try
			{
				auto persisted =
					EncodeNormalizedHostInterfaceSettings(a_settings);
				persisted.hotkeys = a_hotkeyOverrides[0];
				persisted.gamepadHotkeys = a_hotkeyOverrides[1];
				const auto result = PersistHostInterfaceSettings(
					ConfigPath(),
					persisted);
				if (!result.saved)
				{
					a_error = result.detail;
					RecordSaveFailure(a_error);
					return false;
				}
				if (result.usedCrossVolumeFallback)
				{
					REX::WARN(
						"DearModdingUI: {} The fallback is copy-and-delete, not an atomic rename."sv,
						result.detail.English());
				}
				else if (result.temporaryCleanupFailed)
				{
					REX::WARN("DearModdingUI: {}"sv, result.detail.English());
				}
				RecordSaveSuccess();
				return true;
			}
			catch (const std::exception& error)
			{
				a_error = Support::Message::Text(error.what());
				RecordSaveFailure(a_error);
				return false;
			}
			catch (...)
			{
				a_error = Support::Message{ kPersistFailedUnknown, "DearModdingUI.toml" };
				RecordSaveFailure(a_error);
				return false;
			}
		}

		void StorePageActive(bool a_active) noexcept
		{
			const auto current = s_pageActive.load(std::memory_order_acquire);
			if (current == a_active)
				return;
			const std::scoped_lock lock{ s_previewMutex };
			if (current && !a_active)
			{
				s_preview.reset();
				s_pageRevision.fetch_add(1, std::memory_order_release);
				s_revision.Advance();
			}
			s_pageActive.store(a_active, std::memory_order_release);
		}

		[[nodiscard]] bool PersistHotkeyChange(
			HotkeySlot a_slot, std::map<std::string, std::string> a_previous) noexcept
		{
			Support::Message error;
			{
				const std::scoped_lock lock{ s_settingsMutex };
				auto updated = s_hotkeyOverrides;
				updated[static_cast<size_t>(a_slot)] = Hotkeys::Overrides(a_slot);
				if (SaveSettings(s_settings, updated, error))
				{
					s_hotkeyOverrides = std::move(updated);
					return true;
				}
			}
			Hotkeys::InitializeOverrides(std::move(a_previous), a_slot);
			(void)SetHostStatus(DMUI_STATUS_SEVERITY_ERROR, Localization::Render(error));
			return false;
		}
	}

	void Initialize() noexcept
	{
		EnsureLoaded();
	}

	uint64_t Revision() noexcept
	{
		EnsureLoaded();
		return s_revision.Current();
	}

	const HostInterfaceSettings& Current() noexcept
	{
		thread_local Support::Derived<uint64_t, HostInterfaceSettings> cache;
		return cache.Get(Revision(), []() {
			const std::scoped_lock lock{ s_settingsMutex };
			return s_settings;
		});
	}

	HostInterfacePreviewSettings EffectivePreview() noexcept
	{
		thread_local Support::Derived<uint64_t, HostInterfacePreviewSettings> cache;
		return cache.Get(Revision(), []() noexcept {
			{
				const std::scoped_lock lock{ s_previewMutex };
				if (s_preview)
					return *s_preview;
			}
			const std::scoped_lock lock{ s_settingsMutex };
			return PreviewHostInterfaceSettings(s_settings);
		});
	}

	bool Apply(HostInterfaceSettings a_settings) noexcept
	{
		EnsureLoaded();
		a_settings = NormalizeHostInterfaceSettings(std::move(a_settings));
		Support::Message error;
		{
			const std::scoped_lock lock{ s_settingsMutex };
			if (a_settings == s_settings)
			{
				(void)SetHostStatus(
					DMUI_STATUS_SEVERITY_SUCCESS,
					Localization::Text(kSettingsSavedText));
				return true;
			}
			if (!SaveSettings(a_settings, s_hotkeyOverrides, error))
			{
				REX::WARN(
					"DearModdingUI: interface settings could not be persisted: {}"sv,
					error.English());
				(void)SetHostStatus(DMUI_STATUS_SEVERITY_ERROR, Localization::Render(error));
				return false;
			}
			s_settings = std::move(a_settings);
			UpdateMenuToggleChord();
			s_revision.Advance();
		}
		(void)SetHostStatus(
			DMUI_STATUS_SEVERITY_SUCCESS,
			Localization::Text(kSettingsSavedText));
		return true;
	}

	bool SetSidebarLayout(SidebarLayoutKind a_layout) noexcept
	{
		EnsureLoaded();
		const auto layout = NormalizeUserSidebarLayout(a_layout);
		Support::Message error;
		auto saved = true;
		{
			const std::scoped_lock lock{ s_settingsMutex };
			if (s_settings.sidebarLayout != layout)
			{
				auto updated = s_settings;
				updated.sidebarLayout = layout;
				if (SaveSettings(updated, s_hotkeyOverrides, error))
				{
					s_settings = std::move(updated);
					s_revision.Advance();
				}
				else
					saved = false;
			}
		}
		if (!saved)
		{
			REX::WARN(
				"DearModdingUI: sidebar layout could not be persisted: {}"sv,
				error.English());
			(void)SetHostStatus(DMUI_STATUS_SEVERITY_ERROR, Localization::Render(error));
			return false;
		}

		const std::scoped_lock lock{ s_previewMutex };
		if (s_preview && s_preview->sidebarLayout != layout)
		{
			s_preview->sidebarLayout = layout;
			s_revision.Advance();
		}
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
		if (s_preview == a_settings)
			return;
		s_preview = a_settings;
		s_revision.Advance();
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

	bool SetHotkeyOverride(
		std::string_view a_id,
		std::string_view a_chord, HotkeySlot a_slot) noexcept
	{
		EnsureLoaded();
		auto previous = Hotkeys::Overrides(a_slot);
		const auto result = Hotkeys::SetOverride(a_id, a_chord, a_slot);
		if (result != DMUI_RESULT_OK)
			return false;
		return PersistHotkeyChange(a_slot, std::move(previous));
	}

	bool RemoveHotkeyOverride(std::string_view a_id, HotkeySlot a_slot) noexcept
	{
		EnsureLoaded();
		auto previous = Hotkeys::Overrides(a_slot);
		if (!Hotkeys::RemoveOverride(a_id, a_slot))
			return false;
		return PersistHotkeyChange(a_slot, std::move(previous));
	}
}
