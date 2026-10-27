#include <DearModdingUI/navigation/CommandPalette.h>

#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/controls/TextInput.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/localization/Localization.h>
#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/presentation/Theme.h>

#include <REX/REX.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <ranges>
#include <vector>

namespace DearModdingUI
{
	using namespace std::literals;

	namespace
	{
		// ImGui hashes from "###", so the bare suffix names the titled popup.
		constexpr const char* kPopupId{ "###DearModdingPalette" };

		[[nodiscard]] std::string CommandPalettePopupTitle()
		{
			return std::string{ Localization::Text(
					   "$DMUI_Palette_SearchTitle",
					   "Search mods, pages, and actions") } +
				kPopupId;
		}

		[[nodiscard]] std::vector<NavigationSearchHit> BuildResults(
			const NavigationModel& a_model,
			const ClientSelectionState& a_selection,
			const CommandPaletteState& a_state)
		{
			if (!a_state.query.empty())
				return SearchNavigation(a_model, a_state.query);

			std::vector<NavigationSearchHit> results;
			results.reserve(a_selection.recentPages.size());
			const auto searchIndex = a_model.SearchIndex();
			for (const auto page : a_selection.recentPages)
			{
				const auto found = std::ranges::find_if(
					searchIndex,
					[&](const auto& a_record) {
						return a_record.entry.kind ==
								NavigationItemKind::kPage &&
							a_record.entry.page == page;
					});
				if (found != searchIndex.end())
					results.push_back({ &*found });
			}
			return results;
		}

		[[nodiscard]] std::string RowText(
			const NavigationSearchEntry& a_entry)
		{
			std::string label{ a_entry.displayName };
			if (a_entry.kind == NavigationItemKind::kClient)
			{
				label.append(" \xE2\x80\x94 ");
				label.append(Localization::Text("$DMUI_Palette_ModKind", "Mod"));
				return label;
			}
			label.append(" \xE2\x80\x94 ");
			label.append(a_entry.clientDisplayName);
			label.append(" \xE2\x80\xBA ");
			label.append(
				a_entry.category.empty() ?
					Localization::Text("$DMUI_Palette_ActionsCategory", "Actions") :
					a_entry.category.c_str());
			return label;
		}

		[[nodiscard]] std::string RowId(
			const NavigationSearchEntry& a_entry)
		{
			std::string label{ "###DearModdingPalette/" };
			switch (a_entry.kind)
			{
			case NavigationItemKind::kClient:
				label.append("client/");
				break;
			case NavigationItemKind::kAction:
				label.append("action/");
				break;
			default:
				label.append("page/");
				break;
			}
			label.append(a_entry.clientId);
			label.push_back('/');
			label.append(a_entry.id);
			return label;
		}

		[[nodiscard]] std::vector<CommandPaletteRow> BuildRows(
			const NavigationModel& a_model,
			const ClientSelectionState& a_selection,
			const CommandPaletteState& a_state,
			const CommandPaletteActions& a_actions)
		{
			std::vector<CommandPaletteRow> rows;
			for (const auto& hit : BuildResults(a_model, a_selection, a_state))
			{
				const auto& entry = hit.Entry();
				if (entry.kind == NavigationItemKind::kAction &&
					!a_actions.runnable(entry.action))
					continue;
				rows.push_back({ hit, RowId(entry), RowText(entry) });
			}
			return rows;
		}
	}

	const char* CommandPaletteHint() noexcept
	{
		return Localization::Text("$DMUI_Palette_SearchHint", "Search mods, pages, and actions...");
	}

	const NavigationSearchEntry* DrawCommandPalette(
		const NavigationModel& a_model,
		const ClientSelectionState& a_selection,
		CommandPaletteState& a_state,
		const CommandPaletteActions& a_actions) noexcept
	{
		if (a_state.openRequested)
		{
			a_state.query.clear();
			a_state.rows.Invalidate();
			a_state.selection = 0;
			a_state.focusRequested = true;
			ImGui::OpenPopup(kPopupId);
			a_state.openRequested = false;
		}
		if (!ImGui::IsPopupOpen(kPopupId))
		{
			a_state.visible = false;
			return nullptr;
		}

		const auto& style = ImGui::GetStyle();
		ImGui::SetNextWindowSize(
			{
				ImGui::GetWindowSize().x - style.WindowPadding.x * 2.0f,
				ImGui::GetFrameHeightWithSpacing() * 2.0f +
					ImGui::GetTextLineHeightWithSpacing() *
						static_cast<float>(kRecentPageCapacity + 1) +
					style.WindowPadding.y * 2.0f
			},
			ImGuiCond_Appearing);
		auto open = true;
		const auto popupTitle = CommandPalettePopupTitle();
		if (!BeginPopupModalWithRoundedTitleBarButtons(
				popupTitle.c_str(),
				&open,
				ImGuiWindowFlags_NoSavedSettings))
		{
			a_state.visible = false;
			return nullptr;
		}
		a_state.visible = true;
		if (a_state.focusRequested)
		{
			ImGui::SetKeyboardFocusHere();
			a_state.focusRequested = false;
		}

		bool queryEdited{};
		const auto searchResult = DrawTextField(
			"NavigationPaletteSearch",
			a_state.query,
			{ .hint = CommandPaletteHint(), .style = DMUI_TEXT_FIELD_STYLE_SEARCH },
			queryEdited);
		if (searchResult != DMUI_RESULT_OK)
		{
			REX::ERROR("DearModdingUI: command palette search failed ({})"sv,
				static_cast<uint32_t>(searchResult));
			ImGui::TextUnformatted(Localization::Text(
				"$DMUI_Palette_SearchUnavailable",
				"Search input unavailable."));
		}
		const auto* cached = a_state.rows.CachedKey();
		const auto queryChanged = !cached || cached->query != a_state.query;
		const auto& results = a_state.rows.Get(
			CommandPaletteRowsKey::View{
				a_state.query,
				a_selection.recentPages,
				a_actions.revision
			},
			[&]() { return BuildRows(a_model, a_selection, a_state, a_actions); });
		a_state.selection = ResolvePaletteSelectionIndex(
			a_state.selection,
			results.size(),
			queryChanged);

		auto keyboardMoved = false;
		if (!results.empty() && ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
		{
			if (a_state.selection > 0)
				--a_state.selection;
			keyboardMoved = true;
		}
		if (!results.empty() && ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
		{
			a_state.selection = (std::min)(
				a_state.selection + 1,
				results.size() - 1);
			keyboardMoved = true;
		}
		const auto escapePressed =
			ImGui::IsKeyPressed(ImGuiKey_Escape, false) &&
			ConsumeMenuEscapeTarget(MenuEscapeTarget::kPopup);
		auto activated = results.size();
		if (!escapePressed && !results.empty() &&
			ImGui::IsKeyPressed(ImGuiKey_Enter, false))
			activated = a_state.selection;

		ImGui::Spacing();
		{
			const Theme::FontGuard font{ Theme::FontRole::kHeading };
			ImGui::TextUnformatted(
				a_state.query.empty() ?
					Localization::Text("$DMUI_Palette_RecentPages", "Recent pages") :
					Localization::Text("$DMUI_Palette_Results", "Results"));
		}
		ImGui::Separator();
		if (results.empty())
		{
			ImGui::TextDisabled(
				"%s",
				a_state.query.empty() ?
					Localization::Text(
						"$DMUI_Palette_NoRecentPages",
						"No recent pages yet.") :
					Localization::Text(
						"$DMUI_Palette_NoResults",
						"No matching mods, pages, or actions."));
		}
		for (size_t index = 0; index < results.size(); ++index)
		{
			const auto& row = results[index];
			const auto& entry = row.hit.Entry();
			const auto selected = index == a_state.selection;
			const Theme::FontGuard font{ Theme::FontRole::kSubheading };
			const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
			const auto drawn = DrawSelectableRow({
				.id = row.id.c_str(),
				.label = row.text.c_str(),
				.selected = selected,
				.height = ImGui::GetTextLineHeight(),
				.leadingAffordance = RowLeadingAffordance::kIcon,
				.glyph = ResolveNavigationSearchEntryGlyph(entry),
				.textColor = textColor,
				.hoveredTextColor = textColor,
				.trailingWidth = ImGui::GetStyle().FramePadding.x,
				.flushHorizontalHighlight = true
			});
			if (drawn.pressed)
				activated = index;
			if (selected && keyboardMoved)
				ImGui::SetScrollHereY();
		}

		const NavigationSearchEntry* result{};
		if (activated < results.size())
		{
			result = &results[activated].hit.Entry();
			ImGui::CloseCurrentPopup();
			a_state.visible = false;
		}
		else if (escapePressed)
		{
			ImGui::CloseCurrentPopup();
			a_state.visible = false;
		}
		ImGui::EndPopup();
		return result;
	}
}
