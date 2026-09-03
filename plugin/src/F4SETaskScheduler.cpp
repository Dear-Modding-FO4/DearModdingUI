#include <DearModdingUI/MCM/F4SETaskScheduler.h>

#include <F4SE/F4SE.h>

#include <stdexcept>
#include <utility>

namespace DearModdingUI::MCM
{
	void F4SETaskScheduler::Schedule(std::function<void()> a_work)
	{
		const auto* tasks = F4SE::GetTaskInterface();
		if (!tasks)
			throw std::runtime_error("F4SE task interface is unavailable");
		tasks->AddTask(std::move(a_work));
	}
}
