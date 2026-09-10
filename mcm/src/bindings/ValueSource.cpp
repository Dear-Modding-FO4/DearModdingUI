#include <DearModdingUI/MCM/ValueSource.h>

#include <utility>

namespace DearModdingUI::MCM
{
	void NotifyAcceptedModSettingWrite(
		McmEventDispatcher& a_dispatcher,
		std::string_view a_modName,
		const MappedBinding& a_binding) noexcept
	{
		const auto* setting = std::get_if<ModSettingBinding>(&a_binding.source);
		if (setting &&
			setting->declaration != DeclarationState::kUndeclared &&
			!a_binding.descriptorId.empty())
			a_dispatcher.SettingChanged(a_modName, a_binding.descriptorId);
	}

	ValueSnapshot ValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value,
		ValueWriteCompletion a_completion)
	{
		auto result = Write(a_binding, a_value);
		if (a_completion)
		{
			if (const auto* ready = std::get_if<ReadyValue>(&result))
				a_completion(ready->value);
			else
				a_completion(std::unexpected(
					"synchronous value write did not settle successfully"));
		}
		return result;
	}

	void ValueSource::RefreshPage(
		const MappedPage& a_page,
		McmState a_state)
	{
		for (const auto& row : a_page.rows)
		{
			if (row.binding &&
				row.valueRoute == ValueRoute::kSource &&
				Supports(row.binding->Family()) &&
				IsControlOperable(
					a_state,
					row.binding->Family(),
					row.valueRoute))
				(void)Refresh(*row.binding);
		}
	}

	void CompositeValueSource::Add(ValueSource& a_source)
	{
		sources_.push_back(a_source);
	}

	bool CompositeValueSource::Supports(SourceFamily a_family) const noexcept
	{
		return Find(a_family) != nullptr;
	}

	ValueSnapshot CompositeValueSource::Read(
		const MappedBinding& a_binding) const
	{
		const auto* source = Find(a_binding.Family());
		return source ? source->Read(a_binding) : ValueSnapshot{ MissingValue{} };
	}

	uint64_t CompositeValueSource::Refresh(const MappedBinding& a_binding)
	{
		auto* source = Find(a_binding.Family());
		return source ? source->Refresh(a_binding) : 0;
	}

	ValueSnapshot CompositeValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value)
	{
		auto* source = Find(a_binding.Family());
		return source ?
			source->Write(a_binding, a_value) :
			ValueSnapshot{ MissingValue{} };
	}

	ValueSnapshot CompositeValueSource::Write(
		const MappedBinding& a_binding,
		const dmui::SettingValue& a_value,
		ValueWriteCompletion a_completion)
	{
		auto* source = Find(a_binding.Family());
		if (source)
			return source->Write(a_binding, a_value, std::move(a_completion));
		if (a_completion)
			a_completion(std::unexpected(
				"no value source supports this setting"));
		return MissingValue{};
	}

	void CompositeValueSource::RefreshPage(
		const MappedPage& a_page,
		McmState a_state)
	{
		for (const auto source : sources_)
			source.get().RefreshPage(a_page, a_state);
	}

	void CompositeValueSource::Pump() noexcept
	{
		for (const auto source : sources_)
			source.get().Pump();
	}

	ValueSource* CompositeValueSource::Find(
		SourceFamily a_family) const noexcept
	{
		for (const auto source : sources_)
		{
			if (source.get().Supports(a_family))
				return &source.get();
		}
		return nullptr;
	}
}
