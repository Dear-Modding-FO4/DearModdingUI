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

	std::optional<dmui::SettingValue> GlobalValueSource::Read(
		const MappedBinding& a_binding) const noexcept
	{
		try
		{
			const auto* global = Find(a_binding);
			return global ?
				GlobalToSettingValue(global->GetValue(), a_binding.target) :
				std::nullopt;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	void GlobalValueSource::Refresh(const MappedBinding& a_binding) noexcept
	{
		try
		{
			if (a_binding.source.family != SourceFamily::kGlobal ||
				!a_binding.sourceForm)
				return;

			RE::TESGlobal* global{};
			if (const auto reference =
					ParseGlobalFormReference(*a_binding.sourceForm))
			{
				if (auto* data = RE::TESDataHandler::GetSingleton())
				{
					global = data->LookupForm<RE::TESGlobal>(
						reference->localId,
						reference->plugin);
				}
			}
			globals_.insert_or_assign(*a_binding.sourceForm, global);
		}
		catch (...)
		{}
	}

	bool GlobalValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value) noexcept
	{
		try
		{
			auto* global = Find(a_binding);
			const auto value = SettingValueToGlobal(a_value);
			if (!global || !value)
				return false;
			global->value = *value;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	RE::TESGlobal* GlobalValueSource::Find(
		const MappedBinding& a_binding) const noexcept
	{
		if (!a_binding.sourceForm)
			return nullptr;
		const auto found = globals_.find(*a_binding.sourceForm);
		return found == globals_.end() ? nullptr : found->second;
	}
}
