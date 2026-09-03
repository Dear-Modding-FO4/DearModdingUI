#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <RE/B/BSScript_IStackCallbackFunctor.h>
#include <RE/B/BSScript_Variable.h>

#include <functional>
#include <optional>

namespace DearModdingUI::MCM
{
	class PapyrusResultCallback final :
		public RE::BSScript::IStackCallbackFunctor
	{
	public:
		using Function = std::function<void(RE::BSScript::Variable)>;

		explicit PapyrusResultCallback(Function a_function);

		void CallQueued() override;
		void CallCanceled() override;
		void StartMultiDispatch() override;
		void EndMultiDispatch() override;
		void operator()(RE::BSScript::Variable a_result) override;

	private:
		Function function_;
	};

	[[nodiscard]] std::optional<RE::BSScript::Variable> ToPapyrus(
		const dmui::SettingValue& a_value,
		SourceValueKind a_kind);
	[[nodiscard]] std::optional<dmui::SettingValue> FromPapyrus(
		const RE::BSScript::Variable& a_value,
		const dmui::SettingValue& a_target);
}
