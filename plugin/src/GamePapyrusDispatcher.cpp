#include <DearModdingUI/MCM/GamePapyrusDispatcher.h>

#include <DearModdingUI/MCM/PapyrusValue.h>

#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/G/GameScript.h>

#include <memory>
#include <utility>
#include <vector>

namespace DearModdingUI::MCM
{
	bool GamePapyrusDispatcher::DispatchStatic(
		std::string_view a_script,
		std::string_view a_function,
		std::span<const PapyrusArgument> a_arguments,
		const std::optional<dmui::SettingValue>& a_resultTarget,
		PapyrusDispatchCompletion a_completion)
	{
		auto* gameVm = RE::GameVM::GetSingleton();
		auto vm = gameVm ? gameVm->GetVM() : nullptr;
		if (!vm)
			return false;
		std::vector<RE::BSScript::Variable> arguments;
		arguments.reserve(a_arguments.size());
		for (const auto& argument : a_arguments)
		{
			auto converted = ToPapyrus(argument.value, argument.kind);
			if (!converted)
				return false;
			arguments.push_back(std::move(*converted));
		}

		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		if (a_completion)
		{
			auto completion =
				std::make_shared<PapyrusDispatchCompletion>(
					std::move(a_completion));
			callback = RE::BSTSmartPointer<
				RE::BSScript::IStackCallbackFunctor>{
				new PapyrusResultCallback{
					[target = a_resultTarget,
					 completion](
						RE::BSScript::Variable a_result) mutable {
						(*completion)(
							true,
							target ? FromPapyrus(a_result, *target) :
									 std::optional<dmui::SettingValue>{});
					},
					[completion] {
						(*completion)(false, {});
					}
				}
			};
		}
		return vm->DispatchStaticCall(
			RE::BSFixedString{ a_script },
			RE::BSFixedString{ a_function },
			[arguments = std::move(arguments)](
				RE::BSScrapArray<RE::BSScript::Variable>& a_target) {
				a_target.resize(static_cast<uint32_t>(arguments.size()));
				for (uint32_t index = 0; index < arguments.size(); ++index)
					a_target[index] = arguments[index];
				return true;
			},
			callback);
	}
}
