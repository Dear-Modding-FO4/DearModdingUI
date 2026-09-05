#pragma once

#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/DiagnosticReporter.h>
#include <DearModdingUI/MCM/ScaleformInvoker.h>
#include <DearModdingUI/MCM/TaskScheduler.h>

namespace DearModdingUI::MCM
{
	class PapyrusActionExecutor final : public ActionExecutor
	{
	public:
		PapyrusActionExecutor(
			TaskScheduler& a_scheduler,
			ScaleformInvoker& a_scaleform,
			DiagnosticReporter& a_diagnostics);

		[[nodiscard]] std::optional<std::string> UnsupportedReason(
			const Action& a_action) const noexcept override;
		void Execute(
			ActionInvocation a_invocation,
			ActionCompletion a_completion) override;

	private:
		TaskScheduler& scheduler_;
		ScaleformInvoker& scaleform_;
		DiagnosticReporter& diagnostics_;
	};
}
