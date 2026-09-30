#include <DearModdingUI/MCM/F4SETaskScheduler.h>

#include <F4SE/F4SE.h>

#include <Windows.h>

#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		void CALLBACK RunBackground(PTP_CALLBACK_INSTANCE, void* a_context) noexcept
		{
			const std::unique_ptr<std::function<void()>> work{
				static_cast<std::function<void()>*>(a_context)
			};
			try
			{
				(*work)();
			}
			catch (...)
			{}
		}
	}

	void F4SETaskScheduler::Schedule(std::function<void()> a_work)
	{
		const auto* tasks = F4SE::GetTaskInterface();
		if (!tasks)
			throw std::runtime_error("F4SE task interface is unavailable");
		tasks->AddTask(std::move(a_work));
	}

	void F4SETaskScheduler::ScheduleUi(std::function<void()> a_work)
	{
		const auto* tasks = F4SE::GetTaskInterface();
		if (!tasks)
			throw std::runtime_error("F4SE UI task interface is unavailable");
		tasks->AddUITask(std::move(a_work));
	}

	void F4SETaskScheduler::ScheduleBackground(std::function<void()> a_work)
	{
		auto work = std::make_unique<std::function<void()>>(std::move(a_work));
		if (!::TrySubmitThreadpoolCallback(&RunBackground, work.get(), nullptr))
		{
			throw std::system_error(
				static_cast<int>(::GetLastError()),
				std::system_category(),
				"background work could not be queued");
		}
		(void)work.release();
	}
}
