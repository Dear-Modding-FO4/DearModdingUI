#pragma once

#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/TaskScheduler.h>

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace DearModdingUI::MCM
{
	class ValueSource;

	using BoundActionArgument =
		std::variant<bool, int64_t, uint64_t, double, std::string>;

	enum class ActionExecutionStatus : uint8_t
	{
		kSucceeded,
		kFailed,
		kUnsupported
	};

	struct ActionExecutionResult
	{
		ActionExecutionStatus status{ ActionExecutionStatus::kFailed };
		std::optional<std::string> message;
	};

	struct ActionInvocation
	{
		Action action;
		std::optional<dmui::SettingValue> value;
	};

	using ActionCompletion = std::function<void(ActionExecutionResult)>;

	class ActionExecutor
	{
	public:
		ActionExecutor() = default;
		virtual ~ActionExecutor() = default;

		ActionExecutor(const ActionExecutor&) = delete;
		ActionExecutor(ActionExecutor&&) = delete;
		ActionExecutor& operator=(const ActionExecutor&) = delete;
		ActionExecutor& operator=(ActionExecutor&&) = delete;

		[[nodiscard]] virtual std::optional<std::string> UnsupportedReason(
			const Action& a_action) const noexcept = 0;
		virtual void Execute(
			ActionInvocation a_invocation,
			ActionCompletion a_completion) = 0;
	};

	[[nodiscard]] std::expected<
		std::vector<BoundActionArgument>,
		std::string> BindActionArguments(
		const std::vector<ActionArgument>& a_arguments,
		const std::optional<dmui::SettingValue>& a_value) noexcept;

	void ScheduleActionExecution(
		TaskScheduler& a_scheduler,
		std::function<void(const ActionCompletion&)> a_work,
		ActionCompletion a_completion) noexcept;

	// The executor and source outlive the page because its rows capture them by reference.
	void BindActions(
		MappedPage& a_page,
		ActionExecutor& a_executor,
		ValueSource& a_values);
}
