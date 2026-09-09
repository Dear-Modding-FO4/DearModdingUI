#pragma once

#include <DearModdingUI/HostSettings.h>
#include <Support/SubsystemHealth.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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
		PersistedHostInterfaceSettings settings;
		HostSettingsLoadDisposition disposition{
			HostSettingsLoadDisposition::kMissing
		};
		std::string path;
		std::string detail;
		std::vector<std::string> corrections;
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
		std::string detail;
	};

	[[nodiscard]] std::string DescribeWindowsError(uint32_t a_error);

	[[nodiscard]] HostSettingsSaveResult PersistHostInterfaceSettings(
		const std::filesystem::path& a_path,
		const PersistedHostInterfaceSettings& a_settings,
		HostSettingsPersistenceOperations a_operations = {});

	class HostSettingsHealthState
	{
	public:
		void RecordLoad(HostSettingsLoadResult a_result);
		void RecordSaveFailure(std::string_view a_error);
		void RecordSaveSuccess(std::string_view a_path);
		[[nodiscard]] HealthObservation Observation() const;

	private:
		HostSettingsLoadResult load_;
		std::optional<std::string> saveError_;
	};
}
