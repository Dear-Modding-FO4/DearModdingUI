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
