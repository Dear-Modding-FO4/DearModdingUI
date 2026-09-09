#include <DearModdingUI/HostSettings.h>

#include <DearModdingUI/Host.h>
#include <DearModdingUI/HostSettingsHealth.h>
#include <DearModdingUI/Hotkeys.h>
#include <Support/Runtime.h>
#include <Support/SubsystemHealth.h>

#include <REX/REX.h>

#include <atomic>
#include <exception>
#include <filesystem>
#include <format>
#include <mutex>
#include <optional>
#include <string>

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
		std::mutex s_configurationHealthMutex;
		HostSettingsHealthState s_configurationHealthState;

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
						a_snapshot.reason);
					return;
				}
				if (a_snapshot.state == HealthState::kDegraded)
				{
					REX::WARN("[{}] Configuration {}: {}"sv,
						a_snapshot.identity,
						HealthStateLabel(a_snapshot.state),
						a_snapshot.reason);
				}
				else
				{
					REX::INFO("[{}] Configuration {}: {}"sv,
						a_snapshot.identity,
						HealthStateLabel(a_snapshot.state),
						a_snapshot.reason);
				}
			}
		};

		ConfigurationHealthReporter s_configurationHealthReporter;
		SubsystemHealth s_configurationHealth{
			"dmui.configuration",
			s_configurationHealthReporter,
			HostSubsystemHealthRegistry()
		};

		[[nodiscard]] std::filesystem::path ConfigPath()
		{
			return std::filesystem::path{ Addictol::Support::GetRuntimeDirectory() } /
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

		void RecordSaveFailure(std::string_view a_error) noexcept
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

		void EnsureLoaded() noexcept
		{
			std::call_once(s_loadOnce, []() noexcept {
				try
				{
					auto loaded = LoadHostInterfaceSettings(ConfigPath());
					const std::scoped_lock lock{ s_settingsMutex };
					s_settings = loaded.settings;
					s_menuToggleKey.store(
						ParseMenuToggleKey(s_settings.menuToggleKey).virtualKey,
						std::memory_order_release);
					Hotkeys::InitializeOverrides(s_settings.hotkeys);
					Hotkeys::SetReservedVirtualKey(
						s_menuToggleKey.load(std::memory_order_acquire));
					{
						const std::scoped_lock healthLock{
							s_configurationHealthMutex
						};
						s_configurationHealthState.RecordLoad(
							std::move(loaded));
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
						failed.detail = std::format(
							"Could not establish host settings: {}. Using defaults; restart after correcting the configuration.",
							error.what());
						{
							const std::scoped_lock lock{
								s_configurationHealthMutex
							};
							s_configurationHealthState.RecordLoad(
								std::move(failed));
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
						failed.detail =
							"Could not establish host settings. Using defaults; restart after correcting the configuration.";
						{
							const std::scoped_lock lock{
								s_configurationHealthMutex
							};
							s_configurationHealthState.RecordLoad(
								std::move(failed));
						}
						PublishConfigurationHealth();
					}
					catch (...)
					{}
				}
			});
		}

		[[nodiscard]] bool SaveSettings(
			const PersistedHostInterfaceSettings& a_settings,
			std::string& a_error) noexcept
		{
			try
			{
				const auto result = PersistHostInterfaceSettings(
					ConfigPath(),
					a_settings);
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
						result.detail);
				}
				else if (result.temporaryCleanupFailed)
				{
					REX::WARN("DearModdingUI: {}"sv, result.detail);
				}
				RecordSaveSuccess();
				return true;
			}
			catch (const std::exception& error)
			{
				a_error = error.what();
				RecordSaveFailure(a_error);
				return false;
			}
			catch (...)
			{
				a_error = "DearModdingUI.toml could not be persisted.";
				RecordSaveFailure(a_error);
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
