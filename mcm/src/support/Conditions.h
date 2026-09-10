#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <unordered_set>

namespace DearModdingUI::MCM::detail
{
	inline void CollectReferencedControls(
		const GroupCondition& a_condition,
		std::unordered_set<int64_t>& a_controls)
	{
		if (a_condition.type == ConditionType::kControl)
			a_controls.insert(a_condition.control);
		for (const auto& operand : a_condition.operands)
			CollectReferencedControls(operand, a_controls);
	}

	template <class Range>
	[[nodiscard]] std::unordered_set<int64_t> BuildReferencedControls(
		const Range& a_rows)
	{
		std::unordered_set<int64_t> result;
		for (const auto& row : a_rows)
			if (row.groupCondition)
				CollectReferencedControls(*row.groupCondition, result);
		return result;
	}
}
