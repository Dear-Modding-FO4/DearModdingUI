#include <DearModdingUI/MCM/PropertyValueSource.h>

#include <DearModdingUI/MCM/AttachedScriptResolver.h>
#include <DearModdingUI/MCM/PapyrusValue.h>

#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/G/GameScript.h>

#include <atomic>
#include <format>
#include <memory>
#include <optional>
#include <utility>

namespace DearModdingUI::MCM
{
	namespace
	{
		[[nodiscard]] const PropertyBinding* Property(
			const MappedBinding& a_binding) noexcept
		{
			return std::get_if<PropertyBinding>(&a_binding.source);
		}
	}

	PropertyValueSource::PropertyValueSource(TaskScheduler& a_scheduler) :
		scheduler_(a_scheduler)
	{}

	bool PropertyValueSource::Supports(SourceFamily a_family) const noexcept
	{
		return a_family == SourceFamily::kProperty;
	}

	uint64_t PropertyValueSource::Refresh(const MappedBinding& a_binding)
	{
		const auto* property = Property(a_binding);
		const auto key = a_binding.cacheKey;
		const auto generation = Cache().BeginRefresh(key);
		if (!property)
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
				 property = *property,
				 target = a_binding.target] {
					auto [vm, objects] = ResolveAttachedScripts(
						property.form,
						property.scriptName);
					if (!vm || objects.empty())
					{
						QueueCompletion(key, MissingValue{ generation });
						return;
					}

					struct ProbeState
					{
						std::atomic_size_t remaining{};
						std::atomic_bool completed{};
					};
					auto state = std::make_shared<ProbeState>();
					state->remaining = objects.size();
					for (auto& object : objects)
					{
						auto retained = object;
						RE::BSTSmartPointer<
							RE::BSScript::IStackCallbackFunctor> callback{
							new PapyrusResultCallback{
								[this,
								 key,
								 generation,
								 target,
								 retained,
								 state](RE::BSScript::Variable a_result) mutable {
									auto value = FromPapyrus(a_result, target);
									if (value &&
										!state->completed.exchange(true))
									{
										QueueCompletion(
											key,
											ReadyValue{
												std::move(*value),
												generation
											},
											[this, key, retained] {
												const std::scoped_lock lock{
													objectMutex_
												};
												objects_.insert_or_assign(
													key,
													retained);
											});
									}
									if (state->remaining.fetch_sub(1) == 1 &&
										!state->completed.exchange(true))
										QueueCompletion(
											key,
											MissingValue{ generation });
								}
							}
						};
						if (!vm->GetPropertyValue(
								object,
								property.propertyName.c_str(),
								callback))
							(*static_cast<PapyrusResultCallback*>(
								callback.get()))({});
					}
				});
		}
		catch (...)
		{
			(void)Cache().Complete(key, FailedValue{ generation });
		}
		return generation;
	}

	ValueSnapshot PropertyValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value)
	{
		return Write(a_binding, a_value, {});
	}

	ValueSnapshot PropertyValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value,
		ValueWriteCompletion a_completion)
	{
		const auto* property = Property(a_binding);
		const auto argument = ToPapyrus(a_value, a_binding.valueKind);
		const auto key = a_binding.cacheKey;
		if (!property || !argument)
		{
			if (a_completion)
				a_completion(std::unexpected(
					"Papyrus property write is invalid"));
			return Cache().Read(key);
		}
		RE::BSTSmartPointer<RE::BSScript::Object> object;
		{
			const std::scoped_lock lock{ objectMutex_ };
			const auto found = objects_.find(key);
			if (found != objects_.end())
				object = found->second;
		}
		if (!object)
		{
			if (a_completion)
				a_completion(std::unexpected(
					"Papyrus property target is unavailable"));
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
				 object,
				 propertyName = property->propertyName,
				 argument = *argument,
				 resultValue,
				 completion]() mutable {
					auto* gameVm = RE::GameVM::GetSingleton();
					auto vm = gameVm ? gameVm->GetVM() : nullptr;
					auto settled = std::make_shared<std::atomic_bool>();
					auto settle =
						[this,
						 key,
						 generation,
						 settlementToken,
						 resultValue,
						 completion,
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
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>
						callback{
							new PapyrusResultCallback{
								[settle, resultValue](
									RE::BSScript::Variable) mutable {
									settle(resultValue);
								},
								[settle, propertyName, key]() mutable {
									settle(std::unexpected(std::format(
										"Papyrus canceled property '{}' write for '{}'",
										propertyName,
										key)));
								}
							}
						};
					if (!vm ||
						!vm->SetPropertyValue(
							object,
							propertyName.c_str(),
							argument,
							callback))
						settle(std::unexpected(std::format(
							"Papyrus rejected property '{}' write for '{}'",
							propertyName,
							key)));
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
					"Papyrus property write could not be scheduled: {}",
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
					"Papyrus property write could not be scheduled"));
			return Cache().Read(key);
		}
		return stored.snapshot;
	}
}
