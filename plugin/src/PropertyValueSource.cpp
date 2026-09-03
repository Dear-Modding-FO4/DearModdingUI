#include <DearModdingUI/MCM/PropertyValueSource.h>

#include <DearModdingUI/MCM/AttachedScriptResolver.h>
#include <DearModdingUI/MCM/PapyrusValue.h>

#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/G/GameScript.h>

#include <atomic>
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
		const auto* property = Property(a_binding);
		const auto argument = ToPapyrus(a_value, a_binding.valueKind);
		const auto key = a_binding.cacheKey;
		if (!property || !argument)
			return Cache().Read(key);
		RE::BSTSmartPointer<RE::BSScript::Object> object;
		{
			const std::scoped_lock lock{ objectMutex_ };
			const auto found = objects_.find(key);
			if (found != objects_.end())
				object = found->second;
		}
		if (!object)
			return Cache().Read(key);

		auto effective = Cache().Store(key, a_value);
		const auto generation = Generation(effective);
		try
		{
			scheduler_.Schedule(
				[this,
				 key,
				 generation,
				 object,
				 propertyName = property->propertyName,
				 argument = *argument] {
					auto* gameVm = RE::GameVM::GetSingleton();
					auto vm = gameVm ? gameVm->GetVM() : nullptr;
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>
						callback;
					if (!vm ||
						!vm->SetPropertyValue(
							object,
							propertyName.c_str(),
							argument,
							callback))
						QueueCompletion(key, FailedValue{ generation });
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
