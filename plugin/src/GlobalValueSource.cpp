#include <DearModdingUI/MCM/GlobalValueSource.h>

#include <DearModdingUI/MCM/GlobalValue.h>

#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFormUtil.h>

namespace DearModdingUI::MCM
{
	bool GlobalValueSource::Supports(SourceFamily a_family) const noexcept
	{
		return a_family == SourceFamily::kGlobal;
	}

	ValueSnapshot GlobalValueSource::Read(
		const MappedBinding& a_binding) const noexcept
	{
		try
		{
			const std::scoped_lock lock{ mutex_ };
			const auto* global = Find(a_binding);
			const auto* source = Global(a_binding);
			if (!source)
				return FailedValue{};
			const auto entry = globals_.find(source->form);
			if (entry == globals_.end() || !global)
				return MissingValue{
					entry == globals_.end() ? 0 : entry->second.generation
				};
			const auto value =
				GlobalToSettingValue(global->GetValue(), a_binding.target);
			return value ?
				ValueSnapshot{ ReadyValue{
					std::move(*value),
					entry->second.generation
				} } :
				ValueSnapshot{ FailedValue{ entry->second.generation } };
		}
		catch (...)
		{
			return FailedValue{};
		}
	}

	uint64_t GlobalValueSource::Refresh(
		const MappedBinding& a_binding) noexcept
	{
		try
		{
			const std::scoped_lock lock{ mutex_ };
			const auto* source = Global(a_binding);
			if (!source)
				return 0;

			RE::TESGlobal* global{};
			if (const auto reference =
					ParseGlobalFormReference(source->form))
			{
				if (auto* data = RE::TESDataHandler::GetSingleton())
				{
					global = data->LookupForm<RE::TESGlobal>(
						reference->localId,
						reference->plugin);
				}
			}
			auto& entry = globals_[source->form];
			entry.global = global;
			return ++entry.generation;
		}
		catch (...)
		{
			return 0;
		}
	}

	ValueSnapshot GlobalValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value) noexcept
	{
		try
		{
			const std::scoped_lock lock{ mutex_ };
			auto* global = Find(a_binding);
			const auto* source = Global(a_binding);
			const auto value = SettingValueToGlobal(a_value);
			if (!global || !source)
				return MissingValue{};
			auto& entry = globals_[source->form];
			if (!value)
				return FailedValue{ entry.generation };
			global->value = *value;
			++entry.generation;
			auto effective =
				GlobalToSettingValue(global->GetValue(), a_binding.target);
			return effective ?
				ValueSnapshot{ ReadyValue{
					std::move(*effective),
					entry.generation
				} } :
				ValueSnapshot{ FailedValue{ entry.generation } };
		}
		catch (...)
		{
			return FailedValue{};
		}
	}

	RE::TESGlobal* GlobalValueSource::Find(
		const MappedBinding& a_binding) const noexcept
	{
		const auto* source = Global(a_binding);
		if (!source)
			return nullptr;
		const auto found = globals_.find(source->form);
		return found == globals_.end() ? nullptr : found->second.global;
	}

	const GlobalBinding* GlobalValueSource::Global(
		const MappedBinding& a_binding) const noexcept
	{
		return std::get_if<GlobalBinding>(&a_binding.source);
	}
}
