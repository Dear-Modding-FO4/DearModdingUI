#pragma once

#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/TaskScheduler.h>

namespace DearModdingUI::MCM
{
	class PapyrusActionExecutor final : public ActionExecutor
	{
	public:
		explicit PapyrusActionExecutor(TaskScheduler& a_scheduler);

		[[nodiscard]] std::optional<std::string> UnsupportedReason(
			const Action& a_action) const noexcept override;
		void Execute(
			ActionInvocation a_invocation,
			ActionCompletion a_completion) override;

	private:
		TaskScheduler& scheduler_;
	};
}
