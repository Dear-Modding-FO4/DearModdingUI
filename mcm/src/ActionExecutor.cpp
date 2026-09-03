#include <DearModdingUI/MCM/ActionExecutor.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include <charconv>
#include <format>
#include <limits>
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
						else if constexpr (std::integral<T>)
							return BoundActionArgument{ a_current != 0 };
						else if constexpr (std::same_as<T, double>)
							return BoundActionArgument{ a_current != 0.0 };
						else
						{
							int64_t parsed{};
							const auto converted = std::from_chars(
								a_current.data(),
								a_current.data() + a_current.size(),
								parsed);
							if (converted.ec == std::errc{} &&
								converted.ptr ==
									a_current.data() + a_current.size())
								return BoundActionArgument{ parsed != 0 };
						}
						break;
					case SourceValueKind::kInt:
						if constexpr (
							std::same_as<T, int64_t> ||
							std::same_as<T, uint64_t>)
							return BoundActionArgument{ a_current };
						else if constexpr (std::same_as<T, bool>)
							return BoundActionArgument{
								static_cast<int64_t>(a_current)
							};
						else if constexpr (std::same_as<T, double>)
						{
							if (a_current >=
									static_cast<double>(
										(std::numeric_limits<int64_t>::min)()) &&
								a_current <=
									static_cast<double>(
										(std::numeric_limits<int64_t>::max)()))
								return BoundActionArgument{
									static_cast<int64_t>(a_current)
								};
						}
						else
						{
							int64_t parsed{};
							const auto converted = std::from_chars(
								a_current.data(),
								a_current.data() + a_current.size(),
								parsed);
							if (converted.ec == std::errc{} &&
								converted.ptr ==
									a_current.data() + a_current.size())
								return BoundActionArgument{ parsed };
						}
						break;
					case SourceValueKind::kFloat:
						if constexpr (std::same_as<T, double>)
							return BoundActionArgument{ a_current };
						else if constexpr (std::same_as<T, bool> ||
							std::same_as<T, int64_t> ||
							std::same_as<T, uint64_t>)
							return BoundActionArgument{
								static_cast<double>(a_current)
							};
						else
						{
							double parsed{};
							const auto converted = std::from_chars(
								a_current.data(),
								a_current.data() + a_current.size(),
								parsed);
							if (converted.ec == std::errc{} &&
								converted.ptr ==
									a_current.data() + a_current.size())
								return BoundActionArgument{ parsed };
						}
						break;
					case SourceValueKind::kString:
						if constexpr (std::same_as<T, std::string>)
							return BoundActionArgument{ a_current };
						else if constexpr (std::same_as<T, bool>)
							return BoundActionArgument{
								std::string{ a_current ? "true" : "false" }
							};
						else
							return BoundActionArgument{
								std::format("{}", a_current)
							};
						break;
					}
					return std::nullopt;
				},
				a_value);
		}

		[[nodiscard]] std::string ValueText(
			const dmui::SettingValue& a_value)
		{
			return std::visit(
				[](const auto& a_current) {
					using T = std::remove_cvref_t<decltype(a_current)>;
					if constexpr (std::same_as<T, bool>)
						return std::string{ a_current ? "true" : "false" };
					else if constexpr (std::same_as<T, std::string>)
						return a_current;
					else
						return std::format("{}", a_current);
				},
				a_value);
		}

		[[nodiscard]] std::expected<BoundActionArgument, std::string>
			BindTemplate(
			const ValueTemplateArgument& a_argument,
			const dmui::SettingValue& a_value)
		{
			auto value = a_argument.value;
			const auto position = value.find("{value}");
			if (position != std::string::npos)
				value.replace(position, 7, ValueText(a_value));
			switch (a_argument.type)
			{
			case SourceValueKind::kNone:
			case SourceValueKind::kString:
				return BoundActionArgument{ std::move(value) };
			case SourceValueKind::kInt:
			{
				int64_t parsed{};
				const auto converted = std::from_chars(
					value.data(),
					value.data() + value.size(),
					parsed);
				if (converted.ec == std::errc{} &&
					converted.ptr == value.data() + value.size())
					return BoundActionArgument{ parsed };
				break;
			}
			case SourceValueKind::kFloat:
			{
				double parsed{};
				const auto converted = std::from_chars(
					value.data(),
					value.data() + value.size(),
					parsed);
				if (converted.ec == std::errc{} &&
					converted.ptr == value.data() + value.size())
					return BoundActionArgument{ parsed };
				break;
			}
			case SourceValueKind::kBool:
			{
				int64_t parsed{};
				const auto converted = std::from_chars(
					value.data(),
					value.data() + value.size(),
					parsed);
				if (converted.ec == std::errc{} &&
					converted.ptr == value.data() + value.size())
					return BoundActionArgument{ parsed != 0 };
				break;
			}
			}
			return std::unexpected("substituted value has the wrong type");
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
				if (const auto* placeholder =
						std::get_if<ValueTemplateArgument>(&argument))
				{
					if (!a_value)
					{
						return std::unexpected(std::format(
							"argument {} requires a control value",
							index + 1));
					}
					auto bound = BindTemplate(*placeholder, *a_value);
					if (!bound)
					{
						return std::unexpected(std::format(
							"argument {} {}",
							index + 1,
							bound.error()));
					}
					result.push_back(std::move(*bound));
					continue;
				}
				std::visit(
					[&](const auto& a_typed) {
						using T = std::remove_cvref_t<decltype(a_typed)>;
						if constexpr (
							!std::same_as<T, ValueArgument> &&
							!std::same_as<T, ValueTemplateArgument>)
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

	ActionExecutionResult InvokeExternalFunction(
		ScaleformInvoker& a_invoker,
		const CallExternalFunctionAction& a_action,
		const std::optional<dmui::SettingValue>& a_value) noexcept
	{
		auto arguments = BindActionArguments(a_action.arguments, a_value);
		if (!arguments)
			return { ActionExecutionStatus::kFailed, std::move(arguments.error()) };
		switch (a_invoker.Invoke(
			a_action.plugin,
			a_action.function,
			*arguments))
		{
		case ScaleformInvocationStatus::kSucceeded:
			return { ActionExecutionStatus::kSucceeded, {} };
		case ScaleformInvocationStatus::kNoMovieLoaded:
			return {
				ActionExecutionStatus::kFailed,
				"No suitable loaded UI movie is available for Scaleform invocation"
			};
		case ScaleformInvocationStatus::kPluginNotRegistered:
			return {
				ActionExecutionStatus::kFailed,
				std::format(
					"Scaleform plugin '{}' is not registered in a loaded UI movie",
					a_action.plugin)
			};
		case ScaleformInvocationStatus::kFunctionNotRegistered:
			return {
				ActionExecutionStatus::kFailed,
				std::format(
					"Scaleform function '{}.{}' is not registered",
					a_action.plugin,
					a_action.function)
			};
		case ScaleformInvocationStatus::kInvocationFailed:
			return {
				ActionExecutionStatus::kFailed,
				std::format(
					"Scaleform function '{}.{}' rejected the invocation",
					a_action.plugin,
					a_action.function)
			};
		}
		return {
			ActionExecutionStatus::kFailed,
			"Scaleform invocation returned an unknown result"
		};
	}

	namespace
	{
		void Schedule(
			TaskScheduler& a_scheduler,
			std::function<void(const ActionCompletion&)> a_work,
			ActionCompletion a_completion,
			bool a_ui) noexcept
		{
			try
			{
				auto completion =
					std::make_shared<ActionCompletion>(a_completion);
				auto workItem =
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
					};
				if (a_ui)
					a_scheduler.ScheduleUi(std::move(workItem));
				else
					a_scheduler.Schedule(std::move(workItem));
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
	}

	void ScheduleActionExecution(
		TaskScheduler& a_scheduler,
		std::function<void(const ActionCompletion&)> a_work,
		ActionCompletion a_completion) noexcept
	{
		Schedule(
			a_scheduler,
			std::move(a_work),
			std::move(a_completion),
			false);
	}

	void ScheduleUiActionExecution(
		TaskScheduler& a_scheduler,
		std::function<void(const ActionCompletion&)> a_work,
		ActionCompletion a_completion) noexcept
	{
		Schedule(
			a_scheduler,
			std::move(a_work),
			std::move(a_completion),
			true);
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
