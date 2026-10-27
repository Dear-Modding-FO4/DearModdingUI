#include <DearModdingUI/MCM/Compatibility.h>

#include <utility>
#include <vector>

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

	Support::Message SummarizeActionableCompatibility(const MappedPage& a_page)
	{
		size_t unsupportedActions{};
		size_t undeclaredPersistedSettings{};
		for (const auto& row : a_page.rows)
		{
			if (row.action &&
				row.actionInertReason == InertReason::kUnsupportedAction)
				++unsupportedActions;
			if (row.valueRoute != ValueRoute::kSource || !row.binding)
				continue;
			const auto* setting =
				std::get_if<ModSettingBinding>(&row.binding->source);
			if (setting &&
				setting->declaration == DeclarationState::kUndeclared)
				++undeclaredPersistedSettings;
		}

		std::vector<Support::Message> parts;
		if (unsupportedActions)
			parts.emplace_back(BridgeText::kUnsupportedActions, unsupportedActions);
		if (undeclaredPersistedSettings)
			parts.emplace_back(BridgeText::kUndeclaredSettings, undeclaredPersistedSettings);
		if (parts.empty())
			return {};
		return Support::Message{
			BridgeText::kCompatibilitySummary,
			Support::Message::Join(std::move(parts), ", ")
		};
	}
}
