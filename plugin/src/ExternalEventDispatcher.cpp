#include <DearModdingUI/MCM/ExternalEventDispatcher.h>
#include <DearModdingUI/MCM/ExternalEvents.h>

#include <F4SE/F4SE.h>
#include <RE/B/BSScript_IStackCallbackFunctor.h>
#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/G/GameScript.h>

#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		struct DispatchContext
		{
			std::string event;
			std::vector<RE::BSFixedString> arguments;
		};

		void F4SEAPI DispatchRegistrant(
			uint64_t a_handle,
			const char* a_scriptName,
			const char* a_callbackName,
			void* a_data)
		{
			try
			{
				auto* context = static_cast<DispatchContext*>(a_data);
				auto* gameVm = RE::GameVM::GetSingleton();
				auto vm = gameVm ? gameVm->GetVM() : nullptr;
				if (!context || !vm || !a_scriptName || !a_callbackName)
					return;
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				(void)vm->DispatchMethodCall(
					a_handle,
					RE::BSFixedString{ a_scriptName },
					RE::BSFixedString{ a_callbackName },
					[context](
						RE::BSScrapArray<RE::BSScript::Variable>& a_target) {
						a_target.resize(static_cast<uint32_t>(
							context->arguments.size()));
						for (uint32_t index = 0;
							 index < context->arguments.size();
							 ++index)
							a_target[index] = context->arguments[index];
						return true;
					},
					callback);
			}
			catch (...)
			{}
		}

		void Dispatch(DispatchContext& a_context) noexcept
		{
			try
			{
				if (const auto* papyrus = F4SE::GetPapyrusInterface())
					papyrus->GetExternalEventRegistrations(
						a_context.event,
						&a_context,
						DispatchRegistrant);
			}
			catch (...)
			{}
		}

		[[nodiscard]] std::vector<RE::BSFixedString> ToArguments(
			const std::vector<std::string>& a_arguments)
		{
			std::vector<RE::BSFixedString> result;
			result.reserve(a_arguments.size());
			for (const auto& argument : a_arguments)
				result.emplace_back(argument);
			return result;
		}
	}

	ExternalEventDispatcher::ExternalEventDispatcher(
		TaskScheduler& a_scheduler) :
		scheduler_(a_scheduler)
	{}

	void ExternalEventDispatcher::SettingChanged(
		std::string_view a_modName,
		std::string_view a_controlId) noexcept
	{
		std::vector<RE::BSFixedString> arguments{
			RE::BSFixedString{ a_modName },
			RE::BSFixedString{ a_controlId }
		};
		Schedule("OnMCMSettingChange", arguments);
		Schedule(
			"OnMCMSettingChange|" + std::string{ a_modName },
			std::move(arguments));
	}

	void ExternalEventDispatcher::DispatchEvents(
		std::vector<McmExternalEvent> a_events) noexcept
	{
		try
		{
			for (auto& event : a_events)
				Schedule(
					std::move(event.name),
					ToArguments(event.arguments));
		}
		catch (...)
		{}
	}

	void ExternalEventDispatcher::Schedule(
		std::string a_event,
		std::vector<RE::BSFixedString> a_arguments) noexcept
	{
		try
		{
			scheduler_.Schedule(
				[event = std::move(a_event),
				 arguments = std::move(a_arguments)]() mutable {
					DispatchContext context{
						std::move(event),
						std::move(arguments)
					};
					Dispatch(context);
				});
		}
		catch (...)
		{}
	}
}
