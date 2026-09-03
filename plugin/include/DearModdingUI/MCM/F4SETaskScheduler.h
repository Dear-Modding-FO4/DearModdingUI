#pragma once

#include <DearModdingUI/MCM/TaskScheduler.h>

namespace DearModdingUI::MCM
{
	class F4SETaskScheduler final : public TaskScheduler
	{
	public:
		void Schedule(std::function<void()> a_work) override;
		void ScheduleUi(std::function<void()> a_work) override;
	};
}
