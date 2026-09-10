#include <DearModdingUI/MCM/ModSettingValueSource.h>

#include <array>
#include <atomic>
#include <format>
#include <memory>
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
		PapyrusDispatcher& a_dispatcher,
		DiagnosticReporter& a_diagnostics) :
		CachedAsyncValueSource(a_diagnostics),
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
								bool a_succeeded,
								std::optional<dmui::SettingValue> a_value) {
								QueueCompletion(
									key,
									a_succeeded && a_value ?
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
		return Write(a_binding, a_value, {});
	}

	ValueSnapshot ModSettingValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value,
		ValueWriteCompletion a_completion)
	{
		const auto* setting = Setting(a_binding);
		const auto key = a_binding.cacheKey;
		const auto function = FunctionName("SetModSetting", a_binding.valueKind);
		if (!setting || setting->declaration == DeclarationState::kUndeclared ||
			function.empty() || a_value.index() != a_binding.target.index())
		{
			if (a_completion)
				a_completion(std::unexpected(
					"mod setting write is invalid or unavailable"));
			return Cache().Read(key);
		}

		auto stored = Cache().Store(key, a_value);
		const auto generation = Generation(stored.snapshot);
		const auto settlementToken = stored.settlementToken;
		const auto resultValue = std::get<ReadyValue>(stored.snapshot).value;
		auto completion =
			std::make_shared<ValueWriteCompletion>(std::move(a_completion));
		try
		{
			scheduler_.Schedule(
				[this,
				 key,
				 generation,
				 settlementToken,
				 function,
				 settingId = SettingId(*setting),
				 value = a_value,
				 binding = a_binding,
				 resultValue,
				 completion]() mutable {
					auto settled = std::make_shared<std::atomic_bool>();
					auto settle =
						[this,
						 key,
						 generation,
						 settlementToken,
						 resultValue,
						 completion = std::move(completion),
						 settled](ValueWriteResult a_result) mutable {
							if (settled->exchange(true))
								return;
							const auto succeeded = a_result.has_value();
							QueueWriteCompletion(
								key,
								settlementToken,
								succeeded ?
									ValueSnapshot{ ReadyValue{
										resultValue,
										generation
									} } :
									ValueSnapshot{ FailedValue{ generation } },
								[completion,
								 result = std::move(a_result)]() mutable {
									if (*completion)
										(*completion)(std::move(result));
								});
							try
							{
								scheduler_.ScheduleUi([this] { Pump(); });
							}
							catch (...)
							{
								Pump();
							}
						};
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
							[settle, resultValue, function, key](
								bool a_succeeded,
								std::optional<dmui::SettingValue>) mutable {
								if (a_succeeded)
									settle(resultValue);
								else
									settle(std::unexpected(std::format(
										"Papyrus canceled {} for mod setting '{}'",
										function,
										key)));
							}))
					{
						settle(std::unexpected(std::format(
							"Papyrus rejected {} dispatch for mod setting '{}'",
							function,
							key)));
						return;
					}
					NotifyAcceptedModSettingWrite(events_, modName_, binding);
				});
		}
		catch (const std::exception& a_error)
		{
			if (Cache().CompleteWrite(
					key,
					settlementToken,
					FailedValue{ generation }) &&
				*completion)
				(*completion)(std::unexpected(std::format(
					"mod setting write could not be scheduled: {}",
					a_error.what())));
			return Cache().Read(key);
		}
		catch (...)
		{
			if (Cache().CompleteWrite(
					key,
					settlementToken,
					FailedValue{ generation }) &&
				*completion)
				(*completion)(std::unexpected(
					"mod setting write could not be scheduled"));
			return Cache().Read(key);
		}
		return stored.snapshot;
	}
}
