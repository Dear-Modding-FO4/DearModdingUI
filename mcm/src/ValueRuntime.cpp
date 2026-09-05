#include <DearModdingUI/MCM/ValueSource.h>

#include <cmath>
#include <format>
#include <type_traits>

namespace DearModdingUI::MCM
{
	namespace
	{
		[[nodiscard]] bool Truthy(const dmui::SettingValue& a_value) noexcept
		{
			return std::visit(
				[](const auto& a_item) {
					using T = std::remove_cvref_t<decltype(a_item)>;
					if constexpr (std::same_as<T, bool>)
						return a_item;
					else if constexpr (std::same_as<T, std::string>)
						return !a_item.empty();
					else
						return a_item != T{};
				},
				a_value);
		}
	}

	std::string MakeBindingKey(const MappedBinding& a_binding)
	{
		return std::visit(
			[&](const auto& a_source) {
				using T = std::remove_cvref_t<decltype(a_source)>;
				if constexpr (std::same_as<T, GlobalBinding>)
				{
					return std::format(
						"global:{}:{}",
						a_source.form,
						static_cast<unsigned>(a_binding.valueKind));
				}
				else if constexpr (std::same_as<T, PropertyBinding>)
				{
					return std::format(
						"property:{}:{}:{}:{}",
						a_source.form,
						a_source.scriptName.value_or(""),
						a_source.propertyName,
						static_cast<unsigned>(a_binding.valueKind));
				}
				else
				{
					return std::format(
						"setting:{}:{}:{}",
						a_source.section,
						a_source.key,
						static_cast<unsigned>(a_binding.valueKind));
				}
			},
			a_binding.source);
	}

	ValueSnapshot ValueCache::Read(std::string_view a_key) const
	{
		const std::scoped_lock lock{ mutex_ };
		const auto found = values_.find(a_key);
		return found == values_.end() ?
			ValueSnapshot{ MissingValue{} } :
			found->second.snapshot;
	}

	uint64_t ValueCache::BeginRefresh(std::string_view a_key)
	{
		const std::scoped_lock lock{ mutex_ };
		auto& entry = values_[std::string{ a_key }];
		const auto generation = Generation(entry.snapshot) + 1;
		entry.snapshot = PendingValue{ generation };
		return generation;
	}

	StoredWrite ValueCache::Store(
		std::string_view a_key,
		dmui::SettingValue a_value)
	{
		const std::scoped_lock lock{ mutex_ };
		auto& entry = values_[std::string{ a_key }];
		entry.snapshot = ReadyValue{
			std::move(a_value),
			Generation(entry.snapshot) + 1
		};
		++entry.writeToken;
		return { entry.snapshot, entry.writeToken };
	}

	bool ValueCache::Complete(
		std::string_view a_key,
		ValueSnapshot a_snapshot)
	{
		const std::scoped_lock lock{ mutex_ };
		const auto found = values_.find(a_key);
		if (found == values_.end())
			return false;
		auto& current = found->second.snapshot;
		if (Generation(a_snapshot) != Generation(current))
			return false;
		current = std::move(a_snapshot);
		return true;
	}

	bool ValueCache::CompleteWrite(
		std::string_view a_key,
		uint64_t a_settlementToken,
		ValueSnapshot a_snapshot)
	{
		const std::scoped_lock lock{ mutex_ };
		const auto found = values_.find(a_key);
		if (found == values_.end() ||
			found->second.writeToken != a_settlementToken)
			return false;
		auto& current = found->second.snapshot;
		if (Generation(a_snapshot) == Generation(current))
			current = std::move(a_snapshot);
		return true;
	}

	ConditionResult EvaluateCondition(
		const GroupCondition& a_condition,
		const ConditionValueResolver& a_resolve)
	{
		switch (a_condition.type)
		{
		case ConditionType::kControl:
		{
			const auto value = a_resolve(a_condition.control);
			if (const auto* ready = std::get_if<ReadyValue>(&value))
				return Truthy(ready->value) ?
					ConditionResult::kVisible :
					ConditionResult::kHidden;
			return std::holds_alternative<PendingValue>(value) ?
				ConditionResult::kPending :
				ConditionResult::kUnavailable;
		}
		case ConditionType::kAll:
		{
			auto pending = false;
			auto unavailable = false;
			auto hidden = false;
			for (const auto& operand : a_condition.operands)
			{
				const auto result = EvaluateCondition(operand, a_resolve);
				pending = pending || result == ConditionResult::kPending;
				unavailable =
					unavailable || result == ConditionResult::kUnavailable;
				hidden = hidden || result == ConditionResult::kHidden;
			}
			if (unavailable)
				return ConditionResult::kUnavailable;
			if (hidden)
				return ConditionResult::kHidden;
			return pending ? ConditionResult::kPending : ConditionResult::kVisible;
		}
		case ConditionType::kAny:
		{
			auto pending = false;
			auto unavailable = false;
			auto visible = false;
			for (const auto& operand : a_condition.operands)
			{
				const auto result = EvaluateCondition(operand, a_resolve);
				pending = pending || result == ConditionResult::kPending;
				unavailable =
					unavailable || result == ConditionResult::kUnavailable;
				visible = visible || result == ConditionResult::kVisible;
			}
			if (unavailable)
				return ConditionResult::kUnavailable;
			if (visible)
				return ConditionResult::kVisible;
			return pending ? ConditionResult::kPending : ConditionResult::kHidden;
		}
		case ConditionType::kUnknown:
			return ConditionResult::kUnavailable;
		}
		return ConditionResult::kUnavailable;
	}

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
}
