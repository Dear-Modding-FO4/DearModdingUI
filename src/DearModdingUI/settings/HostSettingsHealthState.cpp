#include <DearModdingUI/settings/HostSettingsHealthState.h>

#include <format>

namespace DearModdingUI
{
	void HostSettingsHealthState::RecordLoad(
		const HostSettingsLoadResult& a_result)
	{
		loadDisposition_ = a_result.disposition;
		loadDetail_ = a_result.detail;
	}

	void HostSettingsHealthState::RecordSaveFailure(std::string_view a_error)
	{
		saveError_ = a_error;
	}

	void HostSettingsHealthState::RecordSaveSuccess(std::string_view a_path)
	{
		saveError_.reset();
		loadDisposition_ = HostSettingsLoadDisposition::kLoaded;
		loadDetail_ = std::format("Saved accepted settings to {}.", a_path);
	}

	HealthObservation HostSettingsHealthState::Observation() const
	{
		HealthObservation observation;
		observation.state =
			loadDisposition_ == HostSettingsLoadDisposition::kMissing ||
				loadDisposition_ == HostSettingsLoadDisposition::kLoaded ?
			HealthState::kReady :
			HealthState::kDegraded;
		observation.reason = loadDetail_;
		if (saveError_)
		{
			observation.state = HealthState::kDegraded;
			if (!observation.reason.empty())
				observation.reason.push_back(' ');
			observation.reason.append(
				"Settings remain active, but the latest save failed: ");
			observation.reason.append(*saveError_);
		}
		return observation;
	}
}
