#include <DearModdingUI/MCM/PapyrusActionExecutor.h>

#include <DearModdingUI/MCM/AttachedScriptResolver.h>
#include <DearModdingUI/MCM/PapyrusValue.h>

#include <RE/B/BSScript_IStackCallbackFunctor.h>
#include <RE/B/BSScript_Internal_IFunction.h>
#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/B/BSScript_ObjectTypeInfo.h>
#include <RE/B/BSScript_TypeInfo.h>
#include <RE/B/BSScript_Variable.h>
#include <RE/G/GameScript.h>

#include <algorithm>
#include <cctype>
#include <expected>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace DearModdingUI::MCM
{
	namespace
	{
		class ActionCallback final :
			public RE::BSScript::IStackCallbackFunctor
		{
		public:
			ActionCallback(
				ActionCompletion a_completion,
				RE::BSTSmartPointer<RE::BSScript::Object> a_object = {}) :
				completion_(std::move(a_completion)),
				object_(std::move(a_object))
			{}

			void CallQueued() override {}
			void CallCanceled() override
			{
				Complete({
					ActionExecutionStatus::kFailed,
					"Papyrus canceled the call"
				});
			}
			void StartMultiDispatch() override {}
			void EndMultiDispatch() override {}
			void operator()(RE::BSScript::Variable) override
			{
				Complete({ ActionExecutionStatus::kSucceeded, {} });
			}

		private:
			void Complete(ActionExecutionResult a_result) noexcept
			{
				try
				{
					if (completion_)
					completion_(std::move(a_result));
					completion_ = {};
					object_ = {};
				}
				catch (...)
				{}
			}

			ActionCompletion completion_;
			RE::BSTSmartPointer<RE::BSScript::Object> object_;
		};

		[[nodiscard]] bool EqualAscii(
			std::string_view a_left,
			std::string_view a_right) noexcept
		{
			return a_left.size() == a_right.size() &&
				std::ranges::equal(
					a_left,
					a_right,
					[](char a_lhs, char a_rhs) {
						return std::tolower(
								static_cast<unsigned char>(a_lhs)) ==
							std::tolower(static_cast<unsigned char>(a_rhs));
					});
		}

		[[nodiscard]] RE::BSScript::IFunction* FindFunction(
			RE::BSScript::ObjectTypeInfo* a_type,
			std::string_view a_name,
			bool a_static) noexcept
		{
			for (auto* type = a_type; type; type = type->GetParent())
			{
				if (a_static)
				{
					for (uint32_t index = 0;
						 index < type->GetNumGlobalFuncs();
						 ++index)
					{
						auto* function = type->GetGlobalFuncIter()[index].func.get();
						if (function &&
							EqualAscii(function->GetName().c_str(), a_name))
							return function;
					}
					continue;
				}
				for (uint32_t index = 0;
					 index < type->GetNumMemberFuncs();
					 ++index)
				{
					auto* function = type->GetMemberFuncIter()[index].func.get();
					if (function &&
						EqualAscii(function->GetName().c_str(), a_name))
						return function;
				}
				for (uint32_t stateIndex = 0;
					 stateIndex < type->GetNumNamedStates();
					 ++stateIndex)
				{
					const auto& state = type->GetNamedStateIter()[stateIndex];
					for (uint32_t index = 0; index < state.GetNumFuncs(); ++index)
					{
						auto* function = state.GetFuncIter()[index].func.get();
						if (function &&
							EqualAscii(function->GetName().c_str(), a_name))
							return function;
					}
				}
			}
			return nullptr;
		}

		[[nodiscard]] std::optional<RE::BSScript::Variable> ToVariable(
			const BoundActionArgument& a_argument)
		{
			return std::visit(
				[](const auto& a_value)
					-> std::optional<RE::BSScript::Variable> {
					using T = std::remove_cvref_t<decltype(a_value)>;
					const auto kind =
						std::same_as<T, bool> ? SourceValueKind::kBool :
						(std::same_as<T, double> ?
							SourceValueKind::kFloat :
							(std::same_as<T, std::string> ?
								SourceValueKind::kString :
								SourceValueKind::kInt));
					return ToPapyrus(dmui::SettingValue{ a_value }, kind);
				},
				a_argument);
		}

		[[nodiscard]] RE::BSScript::TypeInfo::RawType ArgumentType(
			const RE::BSScript::Variable& a_argument)
		{
			using Type = RE::BSScript::TypeInfo::RawType;
			if (a_argument.is<bool>())
				return Type::kBool;
			if (a_argument.is<int32_t>())
				return Type::kInt;
			if (a_argument.is<float>())
				return Type::kFloat;
			if (a_argument.is<RE::BSFixedString>())
				return Type::kString;
			return Type::kNone;
		}

		[[nodiscard]] std::expected<void, std::string> Validate(
			const RE::BSScript::IFunction& a_function,
			const std::vector<RE::BSScript::Variable>& a_arguments)
		{
			if (a_function.GetParamCount() != a_arguments.size())
				return std::unexpected(
					"Papyrus function argument count does not match");
			for (uint32_t index = 0; index < a_arguments.size(); ++index)
			{
				RE::BSFixedString name;
				RE::BSScript::TypeInfo type;
				a_function.GetParam(index, name, type);
				if (type.GetRawType() !=
						RE::BSScript::TypeInfo::RawType::kVar &&
					type.GetRawType() != ArgumentType(a_arguments[index]))
					return std::unexpected(
						"Papyrus function argument type does not match");
			}
			return {};
		}

		[[nodiscard]] std::expected<
			std::vector<RE::BSScript::Variable>,
			std::string>
		MakeArguments(
			const Action& a_action,
			const std::optional<dmui::SettingValue>& a_value)
		{
			const auto& declared = std::visit(
				[](const auto& a_typed)
					-> const std::vector<ActionArgument>& {
					using T = std::remove_cvref_t<decltype(a_typed)>;
					if constexpr (std::same_as<T, RunConsoleCommandAction>)
					{
						static const std::vector<ActionArgument> empty;
						return empty;
					}
					else
					{
						return a_typed.arguments;
					}
				},
				a_action);
			auto bound = BindActionArguments(declared, a_value);
			if (!bound)
				return std::unexpected(std::move(bound.error()));
			std::vector<RE::BSScript::Variable> result;
			result.reserve(bound->size());
			for (const auto& boundArgument : *bound)
			{
				auto converted = ToVariable(boundArgument);
				if (!converted)
					return std::unexpected(
						"Papyrus argument is outside its supported range");
				result.push_back(std::move(*converted));
			}
			return result;
		}

		void Complete(
			const ActionCompletion& a_completion,
			ActionExecutionStatus a_status,
			std::string a_message) noexcept
		{
			try
			{
				if (a_completion)
					a_completion({ a_status, std::move(a_message) });
			}
			catch (...)
			{}
		}

		void Run(
			ActionInvocation a_invocation,
			const ActionCompletion& a_completion)
		{
			if (std::holds_alternative<RunConsoleCommandAction>(
					a_invocation.action))
				return Complete(
					a_completion,
					ActionExecutionStatus::kUnsupported,
					"Console command actions are not supported.");
			if (std::holds_alternative<SendEventAction>(a_invocation.action))
				return Complete(
					a_completion,
					ActionExecutionStatus::kUnsupported,
					"Event actions are not supported.");

			auto* gameVm = RE::GameVM::GetSingleton();
			auto vm = gameVm ? gameVm->GetVM() : nullptr;
			if (!vm)
				return Complete(
					a_completion,
					ActionExecutionStatus::kFailed,
					"Papyrus VM is unavailable");

			auto arguments = MakeArguments(
				a_invocation.action,
				a_invocation.value);
			if (!arguments)
				return Complete(
					a_completion,
					ActionExecutionStatus::kFailed,
					std::move(arguments.error()));

			if (const auto* global =
					std::get_if<CallGlobalFunctionAction>(
						&a_invocation.action))
			{
				RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
				if (!vm->GetScriptObjectType(
						RE::BSFixedString{ global->script },
						type) ||
					!type)
					return Complete(
						a_completion,
						ActionExecutionStatus::kFailed,
						"Papyrus script was not found");
				auto* function = FindFunction(
					type.get(),
					global->function,
					true);
				if (!function)
					return Complete(
						a_completion,
						ActionExecutionStatus::kFailed,
						"Papyrus global function was not found");
				if (auto valid = Validate(*function, *arguments); !valid)
					return Complete(
						a_completion,
						ActionExecutionStatus::kFailed,
						std::move(valid.error()));
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
					new ActionCallback{ a_completion }
				};
				if (!vm->DispatchStaticCall(
						RE::BSFixedString{ global->script },
						RE::BSFixedString{ global->function },
						[arguments = std::move(*arguments)](
							RE::BSScrapArray<RE::BSScript::Variable>& a_target) {
							a_target.resize(
								static_cast<uint32_t>(arguments.size()));
							for (uint32_t index = 0;
								 index < arguments.size();
								 ++index)
								a_target[index] = arguments[index];
							return true;
						},
						callback))
					(*static_cast<ActionCallback*>(callback.get())).CallCanceled();
				return;
			}

			const auto* method =
				std::get_if<CallFunctionAction>(&a_invocation.action);
			if (!method)
				return Complete(
					a_completion,
					ActionExecutionStatus::kUnsupported,
					"Action type is not supported.");
			auto target = ResolveAttachedScripts(
				method->form,
				method->scriptName);
			auto& objects = target.objects;
			if (objects.empty())
				return Complete(
					a_completion,
					ActionExecutionStatus::kFailed,
					"Papyrus target form has no matching attached script");
			for (auto& object : objects)
			{
				auto* function = FindFunction(
					object->GetTypeInfo(),
					method->function,
					false);
				if (!function)
					continue;
				if (auto valid = Validate(*function, *arguments); !valid)
					return Complete(
						a_completion,
						ActionExecutionStatus::kFailed,
						std::move(valid.error()));
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
					new ActionCallback{ a_completion, object }
				};
				if (!vm->DispatchMethodCall(
						object,
						RE::BSFixedString{ method->function },
						[arguments = std::move(*arguments)](
							RE::BSScrapArray<RE::BSScript::Variable>& a_target) {
							a_target.resize(
								static_cast<uint32_t>(arguments.size()));
							for (uint32_t index = 0;
								 index < arguments.size();
								 ++index)
								a_target[index] = arguments[index];
							return true;
						},
						callback))
					(*static_cast<ActionCallback*>(callback.get())).CallCanceled();
				return;
			}
			Complete(
				a_completion,
				ActionExecutionStatus::kFailed,
				"Papyrus member function was not found");
		}
	}

	PapyrusActionExecutor::PapyrusActionExecutor(
		TaskScheduler& a_scheduler,
		ScaleformInvoker& a_scaleform) :
		scheduler_(a_scheduler),
		scaleform_(a_scaleform)
	{}

	std::optional<std::string> PapyrusActionExecutor::UnsupportedReason(
		const Action& a_action) const noexcept
	{
		if (std::holds_alternative<CallFunctionAction>(a_action) ||
			std::holds_alternative<CallGlobalFunctionAction>(a_action) ||
			std::holds_alternative<CallExternalFunctionAction>(a_action))
			return std::nullopt;
		if (std::holds_alternative<RunConsoleCommandAction>(a_action))
			return "Console command actions are not supported.";
		return "Event actions are not supported.";
	}

	void PapyrusActionExecutor::Execute(
		ActionInvocation a_invocation,
		ActionCompletion a_completion)
	{
		if (const auto* external =
				std::get_if<CallExternalFunctionAction>(&a_invocation.action))
		{
			ScheduleUiActionExecution(
				scheduler_,
				[action = *external,
				 value = std::move(a_invocation.value),
				 &scaleform = scaleform_](
					const ActionCompletion& a_scheduledCompletion) mutable {
					const auto result = InvokeExternalFunction(
						scaleform,
						action,
						value);
					if (a_scheduledCompletion)
						a_scheduledCompletion(result);
				},
				std::move(a_completion));
			return;
		}
		ScheduleActionExecution(
			scheduler_,
			[invocation = std::move(a_invocation)](
				const ActionCompletion& a_scheduledCompletion) mutable {
				Run(std::move(invocation), a_scheduledCompletion);
			},
			std::move(a_completion));
	}
}
