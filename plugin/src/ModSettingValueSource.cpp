#include <DearModdingUI/MCM/ModSettingValueSource.h>

#include <DearModdingUI/MCM/PapyrusValue.h>

#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/B/BSScriptUtil.h>
#include <RE/G/GameScript.h>

#include <memory>
#include <optional>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		[[nodiscard]] const ModSettingBinding* Setting(
			const MappedBinding& a_binding) noexcept
		{
			return std::get_if<ModSettingBinding>(&a_binding.source);
		}

		[[nodiscard]] std::string SettingId(const ModSettingBinding& a_setting)
		{
			return a_setting.key + ":" + a_setting.section;
		}

		[[nodiscard]] std::string FunctionName(
			std::string_view a_prefix,
			SourceValueKind a_kind)
		{
			switch (a_kind)
			{
			case SourceValueKind::kBool:
				return std::string{ a_prefix } + "Bool";
			case SourceValueKind::kInt:
				return std::string{ a_prefix } + "Int";
			case SourceValueKind::kFloat:
				return std::string{ a_prefix } + "Float";
			case SourceValueKind::kString:
				return std::string{ a_prefix } + "String";
			case SourceValueKind::kNone:
				return {};
			}
			return {};
		}
	}

	ModSettingValueSource::ModSettingValueSource(
		std::string a_modName,
		McmEventDispatcher& a_events,
		TaskScheduler& a_scheduler) :
		modName_(std::move(a_modName)),
		events_(a_events),
		scheduler_(a_scheduler)
	{}

	bool ModSettingValueSource::Supports(SourceFamily a_family) const noexcept
	{
		return a_family == SourceFamily::kModSetting;
	}

	uint64_t ModSettingValueSource::Refresh(const MappedBinding& a_binding)
	{
		const auto* setting = Setting(a_binding);
		const auto key = a_binding.cacheKey;
		const auto generation = Cache().BeginRefresh(key);
		if (!setting || setting->declaration == DeclarationState::kUndeclared)
		{
			(void)Cache().Complete(key, MissingValue{ generation });
			return generation;
		}

		const auto function = FunctionName("GetModSetting", a_binding.valueKind);
		if (function.empty())
		{
			(void)Cache().Complete(key, FailedValue{ generation });
			return generation;
		}
		try
		{
			scheduler_.Schedule(
				[this,
				 key,
				 generation,
				 function,
				 settingId = SettingId(*setting),
				 target = a_binding.target] {
					auto* gameVm = RE::GameVM::GetSingleton();
					auto vm = gameVm ? gameVm->GetVM() : nullptr;
					if (!vm)
					{
						QueueCompletion(key, FailedValue{ generation });
						return;
					}
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>
						callback{
							new PapyrusResultCallback{
								[this, key, generation, target](
									RE::BSScript::Variable a_result) {
									auto value = FromPapyrus(a_result, target);
									QueueCompletion(
										key,
										value ?
											ValueSnapshot{ ReadyValue{
												std::move(*value),
												generation
											} } :
											ValueSnapshot{
												FailedValue{ generation }
											});
								}
							}
						};
					if (!vm->DispatchStaticCall(
							RE::BSFixedString{ "MCM" },
							RE::BSFixedString{ function },
							callback,
							RE::BSFixedString{ modName_ },
							RE::BSFixedString{ settingId }))
						QueueCompletion(key, FailedValue{ generation });
				});
		}
		catch (...)
		{
			(void)Cache().Complete(key, FailedValue{ generation });
		}
		return generation;
	}

	ValueSnapshot ModSettingValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value)
	{
		const auto* setting = Setting(a_binding);
		const auto key = a_binding.cacheKey;
		if (!setting || setting->declaration == DeclarationState::kUndeclared)
			return Cache().Read(key);
		const auto argument = ToPapyrus(a_value, a_binding.valueKind);
		const auto function = FunctionName("SetModSetting", a_binding.valueKind);
		if (!argument || function.empty())
			return Cache().Read(key);

		auto effective = Cache().Store(key, a_value);
		const auto generation = Generation(effective);
		try
		{
			scheduler_.Schedule(
				[this,
				 key,
				 generation,
				 function,
				 settingId = SettingId(*setting),
				 argument = *argument,
				 binding = a_binding] {
					auto* gameVm = RE::GameVM::GetSingleton();
					auto vm = gameVm ? gameVm->GetVM() : nullptr;
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>
						callback;
					if (!vm ||
						!vm->DispatchStaticCall(
							RE::BSFixedString{ "MCM" },
							RE::BSFixedString{ function },
							[&](RE::BSScrapArray<RE::BSScript::Variable>&
									a_arguments) {
								a_arguments.resize(3);
								a_arguments[0] = RE::BSFixedString{ modName_ };
								a_arguments[1] = RE::BSFixedString{ settingId };
								a_arguments[2] = argument;
								return true;
							},
							callback))
					{
						QueueCompletion(key, FailedValue{ generation });
						return;
					}
					NotifyAcceptedModSettingWrite(events_, modName_, binding);
				});
		}
		catch (...)
		{
			(void)Cache().Complete(key, FailedValue{ generation });
			return Cache().Read(key);
		}
		return effective;
	}
}
