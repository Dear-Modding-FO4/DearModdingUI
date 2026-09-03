#include <DearModdingUI/MCM/ModSettingValueSource.h>

#include <array>
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
		TaskScheduler& a_scheduler,
		PapyrusDispatcher& a_dispatcher) :
		modName_(std::move(a_modName)),
		events_(a_events),
		scheduler_(a_scheduler),
		dispatcher_(a_dispatcher)
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
					const std::array<PapyrusArgument, 2> arguments{
						PapyrusArgument{ modName_, SourceValueKind::kString },
						PapyrusArgument{ settingId, SourceValueKind::kString }
					};
					if (!dispatcher_.DispatchStatic(
							"MCM",
							function,
							arguments,
							target,
							[this, key, generation](
								std::optional<dmui::SettingValue> a_value) {
								QueueCompletion(
									key,
									a_value ?
										ValueSnapshot{ ReadyValue{
											std::move(*a_value),
											generation
										} } :
										ValueSnapshot{ FailedValue{ generation } });
							}))
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
		const auto function = FunctionName("SetModSetting", a_binding.valueKind);
		if (!setting || setting->declaration == DeclarationState::kUndeclared ||
			function.empty() || a_value.index() != a_binding.target.index())
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
				 value = a_value,
				 binding = a_binding] {
					const std::array<PapyrusArgument, 3> arguments{
						PapyrusArgument{ modName_, SourceValueKind::kString },
						PapyrusArgument{ settingId, SourceValueKind::kString },
						PapyrusArgument{ value, binding.valueKind }
					};
					if (!dispatcher_.DispatchStatic(
							"MCM",
							function,
							arguments,
							std::nullopt,
							{}))
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
