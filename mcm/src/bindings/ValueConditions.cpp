#include <DearModdingUI/MCM/ValueSource.h>

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

}
