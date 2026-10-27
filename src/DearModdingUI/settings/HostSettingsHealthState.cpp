#include <DearModdingUI/settings/HostSettingsHealthState.h>

namespace DearModdingUI
{
	namespace
	{
		constexpr Support::Phrase kSavedAccepted{
			"$DMUI_Config_SavedAccepted",
			"Saved accepted settings to {}."
		};
		constexpr Support::Phrase kSaveFailedActive{
			"$DMUI_Config_SaveFailedActive",
			"Settings remain active, but the latest save failed: {}"
		};
	}

	void HostSettingsHealthState::RecordLoad(
		const HostSettingsLoadResult& a_result)
	{
		loadDisposition_ = a_result.disposition;
		loadDetail_ = a_result.detail;
	}

	void HostSettingsHealthState::RecordSaveFailure(
		const Support::Message& a_error)
	{
		saveError_ = a_error;
	}

	void HostSettingsHealthState::RecordSaveSuccess(std::string_view a_path)
	{
		saveError_.reset();
		loadDisposition_ = HostSettingsLoadDisposition::kLoaded;
		loadDetail_ = Support::Message{ kSavedAccepted, a_path };
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
			observation.reason = Support::Message::Join(
				{ loadDetail_, Support::Message{ kSaveFailedActive, *saveError_ } },
				" ");
		}
		return observation;
	}
}
