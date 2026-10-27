#include <DearModdingUI/controls/Spinner.h>

#include <DearModdingUI/controls/ControlRenderer.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <cmath>

namespace DearModdingUI
{
	DMUI_Result ValidateSpinner(const DMUI_SpinnerDescriptor* a_spinner) noexcept
	{
		if (!a_spinner || !a_spinner->id || a_spinner->id[0] == '\0' ||
			a_spinner->reserved != 0 || !std::isfinite(a_spinner->size) ||
			a_spinner->size < 0.0f)
			return DMUI_RESULT_INVALID_ARGUMENT;
		return DMUI_RESULT_OK;
	}

	void DrawSpinner(const DMUI_SpinnerDescriptor& a_spinner) noexcept
	{
		auto* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return;
		const auto diameter = a_spinner.size > 0.0f ?
			a_spinner.size :
			ImGui::GetFrameHeight();
		const auto min = window->DC.CursorPos;
		const ImRect bounds{ min, { min.x + diameter, min.y + diameter } };
		ImGui::ItemSize(bounds);
		if (ImGui::ItemAdd(
				bounds, window->GetID(a_spinner.id), nullptr, ImGuiItemFlags_NoNav))
			CurrentControlRenderer().DrawSpinner(bounds, ImGui::GetTime());
	}
}
