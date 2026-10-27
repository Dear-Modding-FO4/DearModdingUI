#include <DearModdingUI/controls/Choice.h>

#include <DearModdingUI/controls/ControlRenderer.h>
#include <DearModdingUI/controls/Tooltip.h>

#include <imgui/imgui.h>

#include <climits>
#include <cmath>

namespace DearModdingUI
{
	DMUI_Result ValidateChoice(const DMUI_ChoiceDescriptor* a_choice) noexcept
	{
		if (!a_choice || !a_choice->id || a_choice->id[0] == '\0' ||
			a_choice->reserved != 0 || !std::isfinite(a_choice->width) ||
			a_choice->optionCount > static_cast<uint32_t>(INT_MAX) ||
			(a_choice->optionCount != 0 && !a_choice->options))
			return DMUI_RESULT_INVALID_ARGUMENT;
		for (uint32_t index = 0; index < a_choice->optionCount; ++index)
		{
			const auto& option = a_choice->options[index];
			if (!option.label || option.reserved != 0)
				return DMUI_RESULT_INVALID_ARGUMENT;
		}
		return DMUI_RESULT_OK;
	}

	DMUI_ChoiceResult DrawChoice(const DMUI_ChoiceDescriptor& a_choice) noexcept
	{
		DMUI_ChoiceResult result{ a_choice.selected, 0u, 0u };
		const auto matched = a_choice.selected >= 0 &&
			static_cast<uint32_t>(a_choice.selected) < a_choice.optionCount;
		const auto* preview = matched ?
			a_choice.options[a_choice.selected].label :
			(a_choice.unmatchedLabel ? a_choice.unmatchedLabel : "");
		const auto& renderer = CurrentControlRenderer();

		ImGui::PushID(a_choice.id);
		if (a_choice.width != 0.0f)
			ImGui::SetNextItemWidth(a_choice.width);
		ImGui::BeginDisabled(a_choice.optionCount == 0);
		if (renderer.BeginChoice("##Choice", preview))
		{
			for (uint32_t index = 0; index < a_choice.optionCount; ++index)
			{
				const auto& option = a_choice.options[index];
				const auto selected =
					matched && static_cast<int32_t>(index) == a_choice.selected;
				ImGui::PushID(static_cast<int>(index));
				ImGui::BeginDisabled(option.enabled == 0);
				const auto pressed = renderer.DrawChoiceRow(option.label, selected);
				ImGui::EndDisabled();
				if (option.description && option.description[0] != '\0')
					Tooltip::Item(option.description);
				ImGui::PopID();
				if (pressed && option.enabled != 0 && !selected)
				{
					result = { static_cast<int32_t>(index), 1u, 1u };
				}
			}
			renderer.EndChoice();
		}
		ImGui::EndDisabled();
		ImGui::PopID();
		return result;
	}
}
