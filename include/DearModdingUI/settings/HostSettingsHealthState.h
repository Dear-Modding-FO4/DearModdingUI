#pragma once

#include <DearModdingUI/settings/HostSettingsPersistence.h>
#include <Support/Message.h>
#include <Support/SubsystemHealth.h>

#include <optional>
#include <string_view>

namespace DearModdingUI
{
	class HostSettingsHealthState
	{
	public:
		void RecordLoad(const HostSettingsLoadResult& a_result);
		void RecordSaveFailure(const Support::Message& a_error);
		void RecordSaveSuccess(std::string_view a_path);
		[[nodiscard]] HealthObservation Observation() const;

	private:
		HostSettingsLoadDisposition loadDisposition_{
			HostSettingsLoadDisposition::kMissing
		};
		Support::Message loadDetail_;
		std::optional<Support::Message> saveError_;
	};
}
