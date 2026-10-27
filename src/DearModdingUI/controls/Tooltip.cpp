#include <DearModdingUI/controls/Tooltip.h>

namespace DearModdingUI::Tooltip
{
	bool IsDueForLastItem() noexcept
	{
		return ImGui::IsItemHovered(kItemHoverFlags);
	}

	bool BeginItem() noexcept
	{
		return IsDueForLastItem() && ImGui::BeginTooltip();
	}

	void Item(std::string_view a_text) noexcept
	{
		if (a_text.empty() || !BeginItem())
			return;
		ImGui::TextUnformatted(a_text.data(), a_text.data() + a_text.size());
		ImGui::EndTooltip();
	}
}
