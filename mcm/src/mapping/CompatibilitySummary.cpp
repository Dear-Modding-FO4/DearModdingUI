#include <DearModdingUI/MCM/Compatibility.h>

#include <format>

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

	std::string SummarizeActionableCompatibility(const MappedPage& a_page)
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

		std::string result;
		const auto append =
			[&result](size_t a_count, std::string_view a_label) {
				if (!a_count)
					return;
				if (!result.empty())
					result.append(", ");
				result.append(std::format(
					"{} {}{}",
					a_count,
					a_label,
					a_count == 1 ? "" : "s"));
			};
		append(unsupportedActions, "unsupported action");
		append(
			undeclaredPersistedSettings,
			"undeclared persisted setting");
		if (result.empty())
			return {};
		return "Compatibility: " + result + ".";
	}
}
