#include <DearModdingUI/MCM/Compatibility.h>

namespace DearModdingUI::MCM
{
	PageCompatibilitySummary SummarizeCompatibility(
		const MappedPage& a_page) noexcept
	{
		PageCompatibilitySummary result;
		result.rows = a_page.rows.size();
		result.localUiStateRows = a_page.localUiStateRows;
		for (const auto& row : a_page.rows)
		{
			if (row.binding)
			{
				++result.bindings;
				if (const auto* setting =
						std::get_if<ModSettingBinding>(&row.binding->source);
					setting &&
					setting->declaration == DeclarationState::kUndeclared)
					++result.undeclaredModSettings;
			}
			if (row.unmappedSource)
				++result.unknownBindings;
			if (row.unsupported)
				++result.unsupported;
			if (row.keybindId && row.keybindInertState &&
				row.keybindInertState->governingReason == InertReason::kNone)
				++result.resolvedKeybinds;
			if (row.action)
				++result.actions;
			if (row.image)
				++result.images;
		}
		return result;
	}
}
