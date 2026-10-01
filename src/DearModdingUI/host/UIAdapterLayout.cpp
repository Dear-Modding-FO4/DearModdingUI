#include "UIAdapterInternal.h"
#include <DearModdingUI/UIBindings.generated.h>

#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace DearModdingUI::UI::Bindings
{
	using namespace AdapterInternal;

	DMUI_Result DMUI_CALL GetCursorPos(
		DMUI_ClientHandle a_client, DMUI_Vec2* a_position) noexcept
	{
		if (!a_position)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		*a_position = Stable(ImGui::GetCursorPos());
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL SetCursorPos(
		DMUI_ClientHandle a_client, DMUI_Vec2 a_position) noexcept
	{
		if (!std::isfinite(a_position.x) || !std::isfinite(a_position.y))
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto validation = Validate(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		ImGui::SetCursorPos(Native(a_position));
		return DMUI_RESULT_OK;
	}

	DMUI_Result DMUI_CALL TextAligned(
		DMUI_ClientHandle a_client, float a_alignX, float a_width,
		const char* a_text, size_t a_length) noexcept
	{
		const auto validation = ValidateText(a_client, a_text, a_length);
		if (validation != DMUI_RESULT_OK)
			return validation;
		if (!std::isfinite(a_alignX) || !std::isfinite(a_width))
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return DMUI_RESULT_OK;
		const auto width = (std::max)(
			a_width > 0.0f ? a_width : ImGui::GetContentRegionAvail().x, 1.0f);
		const auto textSize = ImGui::CalcTextSize(a_text, a_text + a_length, false);
		const auto position = window->DC.CursorPos;
		const ImRect bounds{ position, { position.x + width, position.y + textSize.y } };
		const auto id = window->GetID(a_text, a_text + a_length);
		ImGui::ItemSize(bounds);
		if (!ImGui::ItemAdd(bounds, id))
			return DMUI_RESULT_OK;
		const auto clipped = textSize.x > width;
		const auto offset = clipped ? 0.0f :
			(width - textSize.x) * std::clamp(a_alignX, 0.0f, 1.0f);
		const ImVec2 textPosition{ position.x + offset, position.y };
		ImGui::RenderTextEllipsis(window->DrawList, textPosition, bounds.Max,
			bounds.Max.x, a_text, a_text + a_length, &textSize);
		if (clipped && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && ImGui::BeginTooltip())
		{
			ImGui::TextUnformatted(a_text, a_text + a_length);
			ImGui::EndTooltip();
		}
		return DMUI_RESULT_OK;
	}
}
