#pragma once

#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/B/BSScript_Object.h>
#include <RE/B/BSTSmartPointer.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI::MCM
{
	struct AttachedScriptTarget
	{
		RE::BSTSmartPointer<RE::BSScript::IVirtualMachine> vm;
		std::vector<RE::BSTSmartPointer<RE::BSScript::Object>> objects;
	};

	[[nodiscard]] AttachedScriptTarget ResolveAttachedScripts(
		std::string_view a_form,
		const std::optional<std::string>& a_scriptName);
}
