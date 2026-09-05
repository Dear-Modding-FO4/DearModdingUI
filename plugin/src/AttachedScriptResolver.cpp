#include <DearModdingUI/MCM/AttachedScriptResolver.h>

#include <DearModdingUI/MCM/GlobalValue.h>

#include <RE/B/BSScript_IObjectHandlePolicy.h>
#include <RE/B/BSScript_Internal_VirtualMachine.h>
#include <RE/B/BSSpinLock.h>
#include <RE/G/GameScript.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESForm.h>

#include <utility>

namespace DearModdingUI::MCM
{
	AttachedScriptTarget ResolveAttachedScripts(
		std::string_view a_form,
		const std::optional<std::string>& a_scriptName)
	{
		AttachedScriptTarget result;
		auto* gameVm = RE::GameVM::GetSingleton();
		result.vm = gameVm ? gameVm->GetVM() : nullptr;
		const auto reference = ParseGlobalFormReference(a_form);
		auto* data = RE::TESDataHandler::GetSingleton();
		auto* form = reference && data ?
			data->LookupForm(reference->localId, reference->plugin) :
			nullptr;
		if (!result.vm || !form)
			return result;
		const auto& policy = result.vm->GetObjectHandlePolicy();
		const auto handle = policy.GetHandleForObject(
			static_cast<uint32_t>(form->GetFormType()),
			form);
		if (handle == policy.EmptyHandle())
			return result;

		if (a_scriptName)
		{
			RE::BSTSmartPointer<RE::BSScript::Object> object;
			if (result.vm->FindBoundObject(
					handle,
					a_scriptName->c_str(),
					false,
					object,
					false) &&
				object)
				result.objects.push_back(std::move(object));
			return result;
		}

		auto* internal =
			static_cast<RE::BSScript::Internal::VirtualMachine*>(
				result.vm.get());
		const RE::BSAutoLock lock{ internal->attachedScriptsLock };
		const auto found = internal->attachedScripts.find(handle);
		if (found == internal->attachedScripts.end())
			return result;
		for (const auto& attached : found->second)
			if (attached && attached->IsValid())
				result.objects.emplace_back(attached.get());
		return result;
	}
}
