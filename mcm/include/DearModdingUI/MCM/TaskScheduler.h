#pragma once

#include <functional>

namespace DearModdingUI::MCM
{
	class TaskScheduler
	{
	public:
		TaskScheduler() = default;
		virtual ~TaskScheduler() = default;

		TaskScheduler(const TaskScheduler&) = delete;
		TaskScheduler(TaskScheduler&&) = delete;
		TaskScheduler& operator=(const TaskScheduler&) = delete;
		TaskScheduler& operator=(TaskScheduler&&) = delete;

		virtual void Schedule(std::function<void()> a_work) = 0;
	};
}
