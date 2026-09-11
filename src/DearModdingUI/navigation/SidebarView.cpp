#include <DearModdingUI/navigation/SidebarView.h>

#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/presentation/Theme.h>
#if defined(DMUI_PREVIEW)
#include <SidebarPreview.h>
#endif

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <ranges>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr float kSidebarModFontScale{ 0.8f };

		[[nodiscard]] float ClientStatusTrailingWidth(
			const ClientStatus* a_status) noexcept
		{
			return IsPersistentStatus(
				a_status ? a_status->severity : DMUI_STATUS_SEVERITY_INFO) ?
				ImGui::GetFontSize() :
				0.0f;
		}

		void DrawClientStatusDot(
			const ImRect& a_bounds,
			const ClientStatus* a_status,
			bool a_corner = false) noexcept
		{
			if (!a_status || !IsPersistentStatus(a_status->severity))
				return;
			const auto color =
				a_status->severity == DMUI_STATUS_SEVERITY_WARNING ?
					Theme::kStatusPaletteDefaults.warning :
					Theme::kStatusPaletteDefaults.error;
			const auto radius =
				ImGui::GetFontSize() * Theme::kSearchIconStrokeRatio;
			const auto& padding = ImGui::GetStyle().FramePadding;
			ImGui::GetWindowDrawList()->AddCircleFilled(
				a_corner ?
					ImVec2{
						a_bounds.Max.x - (std::max)(padding.x * 0.5f, radius),
						a_bounds.Min.y + (std::max)(padding.y * 0.5f, radius)
					} :
					ImVec2{
						a_bounds.Max.x - padding.x - radius,
						a_bounds.GetCenter().y
					},
				radius,
				ImGui::GetColorU32(color));
		}

		[[nodiscard]] RowResult DrawClientRow(
			const NavigationClient& a_client,
			const ClientStatus* a_status,
			const ClientSelectionState& a_selection,
			SidebarClientRowKind a_kind,
			bool* a_expanded = nullptr) noexcept
		{
			const Theme::FontGuard font{
				Theme::FontRole::kTitle,
				kSidebarModFontScale
			};
			const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
			const auto row = DrawSelectableRow({
				.id = a_client.id.c_str(),
				.label = a_client.displayName.c_str(),
				.selected = a_client.handle == a_selection.activeClient,
				.leadingAffordance =
					a_kind == SidebarClientRowKind::kTree ?
						RowLeadingAffordance::kArrow :
						RowLeadingAffordance::kIcon,
				.expanded =
					a_kind == SidebarClientRowKind::kTree ?
						a_expanded :
						nullptr,
				.glyph = ResolveNavigationClientIconGlyph(a_client),
				.textColor = textColor,
				.hoveredTextColor = textColor,
				.trailingWidth =
					a_kind == SidebarClientRowKind::kRail ?
						0.0f :
						ClientStatusTrailingWidth(a_status),
				.flushHorizontalHighlight =
					a_kind == SidebarClientRowKind::kRail,
				.centerGlyph = a_kind == SidebarClientRowKind::kRail
			});
			DrawClientStatusDot(
				row.rect,
				a_status,
				a_kind == SidebarClientRowKind::kRail);
			if (a_kind == SidebarClientRowKind::kRail &&
				ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
			{
				const auto section = NavigationClientSectionLabel(
					a_client.origin,
					a_client.bridgeSourceLabel);
				ImGui::SetTooltip(
					"%s\n%s",
					a_client.displayName.c_str(),
					section.c_str());
			}
			return row;
		}

		void DrawNavigationSectionHeading(
			const NavigationClientSection& a_section,
			bool& a_expanded) noexcept
		{
			const auto label = NavigationClientSectionLabel(
				a_section.origin,
				a_section.bridgeSourceLabel);
			const auto key = SidebarOriginKey(a_section);
			ImGui::PushID(static_cast<int>(a_section.origin));
			ImGui::PushID(key.second.c_str());
			{
				const Theme::FontGuard font{ Theme::FontRole::kHeading };
				DrawRuledHeading({
					.key = "##DearModdingOriginSection",
					.text = label.c_str(),
					.glyph =
						a_section.origin == DMUI_CLIENT_ORIGIN_NATIVE ?
							PhosphorGlyph::kPuzzlePiece :
							FindPhosphorIconGlyphOrZero("share-network"),
					.expanded = &a_expanded,
					.layout = RuledHeadingLayout::kLeadingRow,
					.ruleStyle = RuledHeadingRuleStyle::kSubordinate
				});
			}
			ImGui::PopID();
			ImGui::PopID();
		}

		void DrawCategoryHeader(
			const char* a_key,
			const NavigationCategory& a_category,
			bool& a_expanded,
			size_t a_count) noexcept
		{
			DrawRuledHeading({
				.key = a_key,
				.text = a_category.displayName.c_str(),
				.glyph = ResolveNavigationCategoryIconGlyph(
					a_category),
				.count = a_count,
				.expanded = &a_expanded,
				.layout = RuledHeadingLayout::kLeadingRow,
				.ruleStyle = RuledHeadingRuleStyle::kSubordinate
			});
		}

		void DrawPageRows(
			const SidebarViewContext& a_context,
			const NavigationClient& a_client,
			const NavigationCategory& a_category) noexcept
		{
			const Theme::FontGuard font{ Theme::FontRole::kSubtext };
			for (const auto& page : a_category.pages)
			{
				const auto failed = PageFailed(page.handle);
				const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
				const auto id = PageRowLabel(a_client, page);
				const auto row = DrawSelectableRow({
					.id = id.c_str(),
					.label = page.displayName.c_str(),
					.selected =
						page.handle == a_context.selection.activePage,
					.leadingAffordance = RowLeadingAffordance::kIcon,
					.textColor = textColor,
					.hoveredTextColor = textColor,
					.disabledColor = failed ?
						std::optional<ImU32>{ ImGui::GetColorU32(
							Theme::kStatusPaletteDefaults.error) } :
						std::nullopt
				});
				if (row.pressed)
					a_context.intent.Offer(
						NavigationRequest::Page(page.handle));
			}
		}

		void DrawExpandedClientPages(
			const SidebarViewContext& a_context,
			const NavigationClient& a_client) noexcept
		{
			const auto start = ImGui::GetCursorScreenPos();
			const auto& style = ImGui::GetStyle();
			const auto indent =
				ImGui::GetFrameHeight() + style.ItemInnerSpacing.x;
			ImGui::Indent(indent);
			DrawSidebarPageList(a_context, a_client);
			ImGui::Unindent(indent);
			const auto bottom =
				ImGui::GetCursorScreenPos().y - style.ItemSpacing.y;
			const auto thickness = style.WindowBorderSize * 0.5f;
			if (bottom > start.y && thickness > 0.0f)
			{
				auto color = style.Colors[ImGuiCol_Border];
				color.w *= Theme::kFeatureHeadingDefaults.minimizedFactor;
				ImGui::GetWindowDrawList()->AddLine(
					{ start.x + style.FramePadding.x, start.y },
					{ start.x + style.FramePadding.x, bottom },
					ImGui::GetColorU32(color),
					thickness);
			}
		}

		void DrawTreeNavigation(
			const SidebarViewContext& a_context) noexcept
		{
			DrawPresentedSidebarClients(
				a_context,
				SidebarClientRowKind::kTree);
		}

		void DrawTwoPaneNavigation(
			const SidebarViewContext& a_context) noexcept
		{
			const auto& style = ImGui::GetStyle();
			const auto compactSpacing = style.ItemSpacing.y * 0.5f;
			if (ImGui::BeginTable(
					"##DearModdingTwoPane",
					2,
					ImGuiTableFlags_Resizable |
						ImGuiTableFlags_SizingStretchSame |
						ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn(
					"Mods",
					ImGuiTableColumnFlags_WidthStretch,
					1.0f);
				ImGui::TableSetupColumn(
					"Pages",
					ImGuiTableColumnFlags_WidthStretch,
					1.0f);
				ImGui::TableNextColumn();
				if (ImGui::BeginChild(
						"##DearModdingModsPane",
						{ 0.0f, -FLT_MIN }))
				{
					ImGui::PushStyleVar(
						ImGuiStyleVar_ItemSpacing,
						{ style.ItemSpacing.x, compactSpacing });
					DrawPresentedSidebarClients(
						a_context,
						SidebarClientRowKind::kList);
					ImGui::PopStyleVar();
				}
				ImGui::EndChild();

				ImGui::TableNextColumn();
				if (ImGui::BeginChild(
						"##DearModdingPagesPane",
						{ 0.0f, -FLT_MIN }))
				{
					DrawSectionHeader("Pages", PhosphorGlyph::kFiles);
					ImGui::Spacing();
					if (const auto* client =
							a_context.model.FindClient(
								a_context.selection.activeClient))
						DrawSidebarPageList(a_context, *client);
					else
						ImGui::TextDisabled(
							"Select a mod to browse its pages.");
				}
				ImGui::EndChild();
				ImGui::EndTable();
			}
		}

		void DrawDrillDownNavigation(
			const SidebarViewContext& a_context) noexcept
		{
			const auto* selectedClient =
				a_context.browsing.drillDown.level ==
						DrillDownLevel::Pages ?
					a_context.model.FindClient(
						a_context.browsing.drillDown.client) :
					nullptr;
			const auto* section = selectedClient ?
				a_context.model.FindSectionForClient(
					selectedClient->handle) :
				nullptr;
			const auto visible = section &&
				std::ranges::any_of(
					a_context.presentation.sections,
					[&](const auto& a_presented) {
						return a_presented.sectionIndex <
								a_context.model.sections.size() &&
							&a_context.model.sections[
								a_presented.sectionIndex] == section;
					});
			if (!visible)
				selectedClient = nullptr;
			if (!selectedClient)
			{
				a_context.browsing.drillDown = TransitionDrillDown(
					a_context.browsing.drillDown,
					DrillDownEvent::Back);
				DrawPresentedSidebarClients(
					a_context,
					SidebarClientRowKind::kList);
				return;
			}
			{
				const Theme::FontGuard font{
					Theme::FontRole::kTitle,
					kSidebarModFontScale
				};
				const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
				const auto row = DrawSelectableRow({
					.id = "##DearModdingDrillDownBack",
					.label = "All Mods",
					.leadingAffordance = RowLeadingAffordance::kBack,
					.textColor = textColor,
					.hoveredTextColor = textColor
				});
				if (row.pressed)
				{
					a_context.browsing.drillDown = TransitionDrillDown(
						a_context.browsing.drillDown,
						DrillDownEvent::Back);
					return;
				}
			}
			ImGui::Spacing();
			DrawSectionHeader(
				selectedClient->displayName.c_str(),
				ResolveNavigationClientIconGlyph(*selectedClient));
			ImGui::Spacing();
			DrawSidebarPageList(a_context, *selectedClient);
		}

		void DrawHostRows(
			const ClientSelectionState& a_selection,
			SidebarNavigationIntent& a_intent) noexcept
		{
			const Theme::FontGuard font{
				Theme::FontRole::kTitle,
				kSidebarModFontScale
			};
			const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
			for (const auto& page : kHostNavigationPages)
			{
				ImGui::PushID(page.id.data());
				const auto row = DrawSelectableRow({
					.id = "##DearModdingHostPage",
					.label = page.displayName.data(),
					.selected = a_selection.activeHostPage == page.kind,
					.leadingAffordance = RowLeadingAffordance::kIcon,
					.glyph = FindPhosphorIconGlyphOrZero(page.iconName),
					.textColor = textColor,
					.hoveredTextColor = textColor
				});
				ImGui::PopID();
				if (row.pressed)
					a_intent.Offer(NavigationRequest::Host(page.kind));
			}
		}

		[[nodiscard]] bool DrawPaletteAffordance() noexcept
		{
			const auto position = ImGui::GetCursorScreenPos();
			const ImVec2 size{
				ImGui::GetContentRegionAvail().x,
				ImGui::GetFrameHeight()
			};
			const auto pressed =
				ImGui::InvisibleButton("##OpenNavigationPalette", size);
			const auto color = ImGui::GetColorU32(
				ImGui::IsItemActive() ?
					ImGuiCol_FrameBgActive :
					(ImGui::IsItemHovered() ?
						ImGuiCol_FrameBgHovered :
						ImGuiCol_FrameBg));
			ImGui::RenderFrame(
				position,
				{ position.x + size.x, position.y + size.y },
				color,
				true,
				ImGui::GetStyle().FrameRounding);
			const auto scale = Theme::SearchScale();
			const auto iconSize = Theme::kSearchIconSize * scale;
			const ImRect iconBounds{
				{
					position.x + ImGui::GetStyle().FramePadding.x,
					position.y + (size.y - iconSize) * 0.5f
				},
				{
					position.x + ImGui::GetStyle().FramePadding.x + iconSize,
					position.y + (size.y + iconSize) * 0.5f
				}
			};
			DrawCenteredIcon(
				ImGui::GetWindowDrawList(),
				PhosphorGlyph::kMagnifyingGlass,
				iconBounds,
				iconSize,
				IconColor(
					ImGui::GetColorU32(ImGuiCol_Text),
					Theme::kSearchIconAlpha));
			constexpr auto hint = "Search mods, pages, and actions...";
			const auto textSize = ImGui::CalcTextSize(hint);
			ImGui::RenderTextEllipsis(
				ImGui::GetWindowDrawList(),
				{
					iconBounds.Max.x +
						ImGui::GetStyle().ItemInnerSpacing.x,
					position.y + (size.y - textSize.y) * 0.5f
				},
				{
					position.x + size.x -
						ImGui::GetStyle().FramePadding.x,
					position.y + size.y
				},
				position.x + size.x -
					ImGui::GetStyle().FramePadding.x,
				hint,
				nullptr,
				&textSize);
			return pressed;
		}

		[[nodiscard]] bool DrawSourceControls(
			const NavigationModel& a_model,
			const NavigationPresentation& a_presentation,
			const ClientSelectionState& a_selection,
			NavigationPresentationKind a_kind,
			NavigationPresentationState& a_state,
			SidebarNavigationIntent& a_intent) noexcept
		{
			if (a_presentation.sourceControls.empty())
				return false;
			auto changed = false;
			if (ImGui::BeginTable(
					"##DearModdingNavigationDestinations",
					static_cast<int>(a_presentation.sourceControls.size()),
					ImGuiTableFlags_SizingStretchSame))
			{
				for (const auto& control : a_presentation.sourceControls)
				{
					ImGui::TableNextColumn();
					ImGui::PushID(control.id.value);
					const auto activated = ImGui::Selectable(
						control.label.data(),
						control.selected,
						ImGuiSelectableFlags_None,
						{
							ImGui::GetContentRegionAvail().x,
							ImGui::GetFrameHeight()
						});
					ImGui::PopID();
					if (ResolveControlledNavigationControlDecision(
							control.selected,
							activated) !=
						ControlledNavigationControlDecision::Activate)
						continue;
					const auto transition =
						ActivateNavigationSourceControl(
							a_kind,
							a_model,
							control.id,
							a_selection,
							a_state);
					assert(transition.accepted);
					changed = changed || transition.presentationChanged;
					if (transition.navigation)
						a_intent.Offer(*transition.navigation);
				}
				ImGui::EndTable();
			}
			ImGui::Spacing();
			return changed;
		}
	}

	void DrawSidebarPageList(
		const SidebarViewContext& a_context,
		const NavigationClient& a_client,
		bool a_indented) noexcept
	{
		const auto indent =
			ImGui::GetFontSize() + ImGui::GetStyle().ItemInnerSpacing.x;
		for (const auto& category : a_client.categories)
		{
			auto expanded = true;
			if (category.HasHeading())
			{
				const auto key = SidebarCategoryKey(a_client, category.id);
				auto state =
					a_context.browsing.categoryExpansion
						.try_emplace(key, true)
						.first;
				{
					const Theme::FontGuard font{
						Theme::FontRole::kHeading
					};
					DrawCategoryHeader(
						key.c_str(),
						category,
						state->second,
						category.pages.size());
				}
				expanded = state->second;
			}
			if (!expanded)
				continue;
			if (a_indented)
				ImGui::Indent(indent);
			DrawPageRows(a_context, a_client, category);
			if (a_indented)
				ImGui::Unindent(indent);
		}
	}

	void DrawPresentedSidebarClients(
		const SidebarViewContext& a_context,
		SidebarClientRowKind a_kind) noexcept
	{
		bool firstSection = true;
		for (const auto& presented : a_context.presentation.sections)
		{
			assert(presented.sectionIndex < a_context.model.sections.size());
			const auto& section =
				a_context.model.sections[presented.sectionIndex];
			auto sectionExpanded = true;
			if (a_kind == SidebarClientRowKind::kRail)
			{
				if (!firstSection)
					ImGui::Separator();
			}
			else if (presented.showHeading)
			{
				auto state =
					a_context.browsing.originExpansion
						.try_emplace(SidebarOriginKey(section), true)
						.first;
				DrawNavigationSectionHeading(section, state->second);
				sectionExpanded = state->second;
			}
			firstSection = false;
			if (!sectionExpanded)
				continue;
			for (const auto clientIndex : section.clientIndices)
			{
				assert(clientIndex < a_context.model.clients.size());
				const auto& client = a_context.model.clients[clientIndex];
				bool* expanded{};
				if (a_kind == SidebarClientRowKind::kTree)
				{
					expanded =
						&a_context.browsing.modExpansion
							.try_emplace(client.id, false)
							.first->second;
				}
				const auto row = DrawClientRow(
					client,
					FindClientStatus(a_context.statuses, client.handle),
					a_context.selection,
					a_kind,
					expanded);
				if (row.pressed)
				{
					if (expanded)
						*expanded = true;
					a_context.intent.Offer(
						NavigationRequest::Client(client.handle));
				}
				if (expanded && *expanded)
					DrawExpandedClientPages(a_context, client);
			}
		}
	}

	SidebarDrawResult DrawSidebar(
		const NavigationModel& a_model,
		std::span<const ClientStatus> a_statuses,
		const ClientSelectionState& a_selection,
		SidebarLayoutKind a_layout,
		NavigationPresentationKind a_presentationKind,
		NavigationPresentationState& a_presentationState,
		SidebarViewState& a_state) noexcept
	{
		SidebarNavigationIntent intent;
		auto openPalette = false;
		ImGui::TableNextColumn();
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());
		if (ImGui::BeginListBox(
				"##DearModdingMenusList",
				{ -FLT_MIN, -FLT_MIN }))
		{
			DrawSectionHeader("Host", PhosphorGlyph::kAppWindow);
			DrawHostRows(a_selection, intent);
			ImGui::Spacing();
			DrawSectionHeader("Mods", PhosphorGlyph::kSquaresFour);
			openPalette = DrawPaletteAffordance();
			ImGui::Spacing();

			auto presentation = BuildNavigationPresentation(
				a_presentationKind,
				a_model,
				a_presentationState);
			const auto presentationChanged = DrawSourceControls(
				a_model,
				presentation,
				a_selection,
				a_presentationKind,
				a_presentationState,
				intent);
			if (presentationChanged)
			{
				presentation = BuildNavigationPresentation(
					a_presentationKind,
					a_model,
					a_presentationState);
			}
			const SidebarViewContext context{
				a_model,
				a_statuses,
				presentation,
				a_selection,
				a_state,
				intent
			};
			switch (a_layout)
			{
			case SidebarLayoutKind::TwoPane:
				DrawTwoPaneNavigation(context);
				break;
			case SidebarLayoutKind::DrillDown:
				DrawDrillDownNavigation(context);
				break;
#if defined(DMUI_PREVIEW)
			case SidebarLayoutKind::IconRail:
				DrawIconRailNavigation(context);
				break;
#endif
			default:
				DrawTreeNavigation(context);
				break;
			}
			ImGui::EndListBox();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		return { intent.request, openPalette };
	}
}
