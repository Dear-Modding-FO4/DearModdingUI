#pragma once

#include <imgui/imgui.h>

#include <string_view>

namespace DearModdingUI::Tooltip
{
	// The one tooltip policy: the theme's normal hover delay or navigation
	// focus, and also while the item is disabled.
	inline constexpr ImGuiHoveredFlags kItemHoverFlags =
		ImGuiHoveredFlags_AllowWhenDisabled |
		ImGuiHoveredFlags_DelayNormal |
		ImGuiHoveredFlags_NoSharedDelay;

	[[nodiscard]] bool IsDueForLastItem() noexcept;
	// Opens the tooltip window when due; pair a true result with ImGui::EndTooltip.
	[[nodiscard]] bool BeginItem() noexcept;
	// Shows plain text for the last item.
	void Item(std::string_view a_text) noexcept;
}
