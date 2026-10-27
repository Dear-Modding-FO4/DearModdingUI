#include <DearModdingUI/controls/Toggle.h>

#include <DearModdingUI/controls/ControlRenderer.h>

#include <imgui/imgui.h>

namespace DearModdingUI
{
	DMUI_Result ValidateToggle(const DMUI_ToggleDescriptor* a_toggle) noexcept
	{
		if (!a_toggle || !a_toggle->id || a_toggle->id[0] == '\0' ||
			a_toggle->reserved != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		return DMUI_RESULT_OK;
	}

	DMUI_ToggleResult DrawToggle(const DMUI_ToggleDescriptor& a_toggle) noexcept
	{
		auto value = a_toggle.value != 0;
		ImGui::PushID(a_toggle.id);
		const auto changed = CurrentControlRenderer().DrawToggle(value, a_toggle.label);
		ImGui::PopID();
		return { value ? 1u : 0u, changed ? 1u : 0u };
	}
}
