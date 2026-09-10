#pragma once

#include <DearModdingUI/settings/HostSettingsPersistence.h>
#include <Support/SubsystemHealth.h>

#include <optional>
#include <string>
#include <string_view>

namespace DearModdingUI
{
	class HostSettingsHealthState
	{
	public:
		void RecordLoad(const HostSettingsLoadResult& a_result);
		void RecordSaveFailure(std::string_view a_error);
		void RecordSaveSuccess(std::string_view a_path);
		[[nodiscard]] HealthObservation Observation() const;

	private:
		HostSettingsLoadDisposition loadDisposition_{
			HostSettingsLoadDisposition::kMissing
		};
		std::string loadDetail_;
		std::optional<std::string> saveError_;
	};
}
