#pragma once

#include <DearModdingUI/settings/HostSettings.h>
#include <Support/Message.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace DearModdingUI
{
	enum class HostSettingsLoadDisposition
	{
		kMissing,
		kLoaded,
		kCorrected,
		kFailed
	};

	struct HostSettingsLoadResult
	{
		HostInterfaceSettings settings;
		std::map<std::string, std::string> hotkeys;
		std::map<std::string, std::string> gamepadHotkeys;
		HostSettingsLoadDisposition disposition{
			HostSettingsLoadDisposition::kMissing
		};
		std::string path;
		Support::Message detail;
		std::vector<Support::Message> corrections;
	};

	[[nodiscard]] HostSettingsLoadResult LoadHostInterfaceSettings(
		const std::filesystem::path& a_path);

	using HostSettingsMoveFile = bool (*)(
		void* a_context,
		const std::filesystem::path& a_source,
		const std::filesystem::path& a_destination,
		uint32_t a_flags,
		uint32_t& a_nativeError) noexcept;

	struct HostSettingsPersistenceOperations
	{
		void* context{ nullptr };
		HostSettingsMoveFile moveFile{ nullptr };
	};

	struct HostSettingsSaveResult
	{
		bool saved{ false };
		bool usedCrossVolumeFallback{ false };
		bool temporaryCleanupFailed{ false };
		uint32_t nativeError{ 0 };
		Support::Message detail;
	};

	// Also used when a save throws before the persister can describe it.
	inline constexpr Support::Phrase kPersistFailedUnknown{
		"$DMUI_Config_PersistFailedUnknown",
		"Persisting {} failed for an unknown reason."
	};

	[[nodiscard]] std::string DescribeWindowsError(uint32_t a_error);

	[[nodiscard]] HostSettingsSaveResult PersistHostInterfaceSettings(
		const std::filesystem::path& a_path,
		const PersistedHostInterfaceSettings& a_settings,
		HostSettingsPersistenceOperations a_operations = {});

}
