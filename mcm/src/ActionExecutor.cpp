#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include <format>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		[[nodiscard]] std::optional<BoundActionArgument> BindValue(
			const dmui::SettingValue& a_value,
			SourceValueKind a_kind)
		{
			return std::visit(
				[a_kind](const auto& a_current)
					-> std::optional<BoundActionArgument> {
					using T = std::remove_cvref_t<decltype(a_current)>;
					switch (a_kind)
					{
					case SourceValueKind::kNone:
						return BoundActionArgument{ a_current };
					case SourceValueKind::kBool:
						if constexpr (std::same_as<T, bool>)
							return BoundActionArgument{ a_current };
						break;
					case SourceValueKind::kInt:
						if constexpr (
							std::same_as<T, int64_t> ||
							std::same_as<T, uint64_t>)
							return BoundActionArgument{ a_current };
						break;
					case SourceValueKind::kFloat:
						if constexpr (std::same_as<T, double>)
							return BoundActionArgument{ a_current };
						break;
					case SourceValueKind::kString:
						if constexpr (std::same_as<T, std::string>)
							return BoundActionArgument{ a_current };
						break;
					}
					return std::nullopt;
				},
				a_value);
		}

		void CompleteAction(
			const ActionCompletion& a_completion,
			ActionExecutionResult a_result) noexcept
		{
			try
			{
				if (a_completion)
					a_completion(std::move(a_result));
			}
			catch (...)
			{}
		}

		struct PendingResult
		{
			std::string row;
			ActionExecutionResult result;
		};

		class PageActionState
		{
		public:
			explicit PageActionState(std::vector<MappedBinding> a_bindings) :
				bindings_(std::move(a_bindings))
			{}

			void Complete(
				std::string a_row,
				ActionExecutionResult a_result) noexcept
			{
				try
				{
					const std::scoped_lock lock{ mutex_ };
					pending_.push_back({
						std::move(a_row),
						std::move(a_result)
					});
				}
				catch (...)
				{}
			}

			void Pump(
				dmui::SettingsPage& a_settings,
				ValueSource& a_values) noexcept
			{
				try
				{
					std::vector<PendingResult> pending;
					{
						const std::scoped_lock lock{ mutex_ };
						pending.swap(pending_);
					}
					auto refresh = false;
					for (auto& completion : pending)
					{
						if (completion.result.status ==
							ActionExecutionStatus::kSucceeded)
						{
							refresh = true;
							continue;
						}
						auto message = std::format(
							"Action '{}': {}",
							completion.row,
							completion.result.message.value_or("execution failed"));
						const auto noteId =
							"dearmodding.mcm.action." + completion.row;
						const auto found = std::ranges::find(
							a_settings.notes,
							noteId,
							&dmui::SettingsPageNote::noteId);
						if (found == a_settings.notes.end())
						{
							a_settings.notes.push_back({
								std::move(message),
								false,
								std::move(noteId)
							});
						}
						else
						{
							found->text = std::move(message);
						}
					}
					if (refresh)
					{
						for (const auto& binding : bindings_)
							if (a_values.Supports(binding.Family()))
								(void)a_values.Refresh(binding);
					}
				}
				catch (...)
				{}
			}

		private:
			std::mutex mutex_;
			std::vector<PendingResult> pending_;
			std::vector<MappedBinding> bindings_;
		};

		void Invoke(
			ActionExecutor& a_executor,
			const Action& a_action,
			std::optional<dmui::SettingValue> a_value,
			std::string a_row,
			const std::shared_ptr<PageActionState>& a_state) noexcept
		{
			try
			{
				a_executor.Execute(
					ActionInvocation{ a_action, std::move(a_value) },
					[row = std::move(a_row), a_state](
						ActionExecutionResult a_result) mutable {
						a_state->Complete(
							std::move(row),
							std::move(a_result));
					});
			}
			catch (const std::exception& a_error)
			{
				a_state->Complete(
					std::move(a_row),
					{
						ActionExecutionStatus::kFailed,
						a_error.what()
					});
			}
			catch (...)
			{
				a_state->Complete(
					std::move(a_row),
					{
						ActionExecutionStatus::kFailed,
						"executor threw an unknown exception"
					});
			}
		}

		[[nodiscard]] dmui::SettingDescriptor* FindSetting(
			dmui::SettingsPage& a_page,
			std::string_view a_id)
		{
			for (auto& group : a_page.groups)
			{
				for (auto& setting : group.settings)
				if (setting.id == a_id)
					return &setting;
			}
			return nullptr;
		}

		[[nodiscard]] dmui::SettingsActionRow* FindAction(
			dmui::SettingsPage& a_page,
			std::string_view a_id)
		{
			for (auto& group : a_page.groups)
			{
				for (auto& action : group.actionRows)
					if (action.id == a_id)
						return &action;
			}
			return nullptr;
		}
	}

	std::expected<std::vector<BoundActionArgument>, std::string>
		BindActionArguments(
		const std::vector<ActionArgument>& a_arguments,
		const std::optional<dmui::SettingValue>& a_value) noexcept
	{
		try
		{
			std::vector<BoundActionArgument> result;
			result.reserve(a_arguments.size());
			for (size_t index = 0; index < a_arguments.size(); ++index)
			{
				const auto& argument = a_arguments[index];
				if (const auto* placeholder =
						std::get_if<ValueArgument>(&argument))
				{
					if (!a_value)
					{
						return std::unexpected(std::format(
							"argument {} requires a control value",
							index + 1));
					}
					auto bound = BindValue(*a_value, placeholder->type);
					if (!bound)
					{
						return std::unexpected(std::format(
							"argument {} value has the wrong type",
							index + 1));
					}
					result.push_back(std::move(*bound));
					continue;
				}
				std::visit(
					[&](const auto& a_typed) {
						using T = std::remove_cvref_t<decltype(a_typed)>;
						if constexpr (!std::same_as<T, ValueArgument>)
							result.emplace_back(a_typed);
					},
					argument);
			}
			return result;
		}
		catch (const std::exception& a_error)
		{
			return std::unexpected(std::string{ a_error.what() });
		}
		catch (...)
		{
			return std::unexpected("argument binding failed");
		}
	}

	void ScheduleActionExecution(
		TaskScheduler& a_scheduler,
		std::function<void(const ActionCompletion&)> a_work,
		ActionCompletion a_completion) noexcept
	{
		try
		{
			auto completion =
				std::make_shared<ActionCompletion>(a_completion);
			a_scheduler.Schedule(
				[work = std::move(a_work),
				 completion = std::move(completion)] {
					try
					{
						work(*completion);
					}
					catch (const std::exception& a_error)
					{
						CompleteAction(
							*completion,
							{
								ActionExecutionStatus::kFailed,
								a_error.what()
							});
					}
					catch (...)
					{
						CompleteAction(
							*completion,
							{
								ActionExecutionStatus::kFailed,
								"action execution threw an unknown exception"
							});
					}
				});
		}
		catch (const std::exception& a_error)
		{
			CompleteAction(
				a_completion,
				{
					ActionExecutionStatus::kFailed,
					a_error.what()
				});
		}
		catch (...)
		{
			CompleteAction(
				a_completion,
				{
					ActionExecutionStatus::kFailed,
					"action scheduling threw an unknown exception"
				});
		}
	}

	void BindActions(
		MappedPage& a_page,
		ActionExecutor& a_executor,
		ValueSource& a_values)
	{
		std::vector<MappedBinding> bindings;
		for (const auto& row : a_page.rows)
			if (row.binding)
				bindings.push_back(*row.binding);
		auto state =
			std::make_shared<PageActionState>(std::move(bindings));
		auto priorPrepare = std::move(a_page.settings.prepareView);
		a_page.settings.prepareView =
			[priorPrepare = std::move(priorPrepare),
			 state,
			 &a_values](dmui::SettingsPage& a_settings) {
				if (priorPrepare)
					priorPrepare(a_settings);
				state->Pump(a_settings, a_values);
			};

		for (const auto& row : a_page.rows)
		{
			if (!row.action)
				continue;
			if (auto* action = FindAction(a_page.settings, row.id))
			{
				if (const auto reason =
						a_executor.UnsupportedReason(*row.action))
				{
					action->description = action->description.empty() ?
						*reason :
						action->description + "\n" + *reason;
					action->isEnabled = [] { return false; };
					continue;
				}
				action->activate =
					[&a_executor,
					 actionValue = *row.action,
					 id = row.id,
					 state] {
						Invoke(
							a_executor,
							actionValue,
							std::nullopt,
							id,
							state);
					};
				continue;
			}

			auto* setting = FindSetting(a_page.settings, row.id);
			if (!setting)
				continue;
			if (const auto reason =
					a_executor.UnsupportedReason(*row.action))
			{
				if (!setting->description.empty())
					setting->description.push_back('\n');
				setting->description += *reason;
				setting->isEnabled = [] { return false; };
				continue;
			}
			if (!setting->binding.set)
				continue;
			auto priorSet = std::move(setting->binding.set);
			setting->binding.set =
				[&a_executor,
				 priorSet = std::move(priorSet),
				 actionValue = *row.action,
				 id = row.id,
				 state](dmui::SettingValue a_value) mutable {
					auto effective = priorSet(std::move(a_value));
					Invoke(
						a_executor,
						actionValue,
						effective,
						id,
						state);
					return effective;
				};
		}
	}
}
