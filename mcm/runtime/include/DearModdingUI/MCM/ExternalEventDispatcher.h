#pragma once

#include <DearModdingUI/MCM/ExternalEvents.h>
#include <DearModdingUI/MCM/ValueSource.h>
#include <DearModdingUI/MCM/TaskScheduler.h>

#include <RE/B/BSFixedString.h>

#include <string>
#include <vector>

namespace DearModdingUI::MCM
{
	class ExternalEventDispatcher final : public McmEventDispatcher
	{
	public:
		explicit ExternalEventDispatcher(TaskScheduler& a_scheduler);

		void SettingChanged(
			std::string_view a_modName,
			std::string_view a_controlId) noexcept override;
		void DispatchEvents(std::vector<McmExternalEvent> a_events) noexcept;

	private:
		void Schedule(
			std::string a_event,
			std::vector<RE::BSFixedString> a_arguments) noexcept;

		TaskScheduler& scheduler_;
	};
}
