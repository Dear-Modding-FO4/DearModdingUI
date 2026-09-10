#include <DearModdingUI/pages/HostPageViews.h>

#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/controls/Faq.h>
#include <DearModdingUI/pages/Health.h>
#include <DearModdingUI/pages/Home.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/settings/HostSettingsView.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/controls/LinkRow.h>
#include <DearModdingUI/host/MenuToggleKey.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/host/Status.h>
#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <format>
#include <ranges>
#include <vector>

namespace DearModdingUI
{
	namespace
	{
		struct HostSettingsTitleButton
		{
			SettingsAction action;
			const char* id;
			const char* label;
			const char* tooltip;
			float width;
		};

		template <class DrawValue>
		void DrawHostDetailRow(
			const char* a_id,
			const char* a_label,
			const char* a_description,
			DrawValue&& a_drawValue) noexcept
		{
			const auto result = SettingsTable::BeginRow(
				DMUI_INVALID_CLIENT_HANDLE,
				a_id,
				a_label,
				a_description);
			if (result.result != DMUI_RESULT_OK || !result.visible)
				return;
			a_drawValue();
			bool resetPressed{};
			(void)SettingsTable::EndRow(
				DMUI_INVALID_CLIENT_HANDLE,
				{ false, false },
				resetPressed);
		}

		[[nodiscard]] size_t CountClientsNeedingAttention(
			const std::vector<RegisteredClient>& a_clients,
			std::span<const ClientStatus> a_statuses) noexcept
		{
			return static_cast<size_t>(std::ranges::count_if(
				a_clients,
				[&](const RegisteredClient& a_client) {
					const auto severity = EffectiveClientStatusSeverity(
						a_client.callbackFailed,
						FindClientStatus(a_statuses, a_client.handle));
					return severity == DMUI_STATUS_SEVERITY_WARNING ||
						severity == DMUI_STATUS_SEVERITY_ERROR;
				}));
		}

		void DrawHome() noexcept
		{
			(void)DrawTitleRow({
				.title = kHostHomePage.displayName.data(),
				.titleScale = Theme::kFeatureTitleScale,
				.summary = kHostHomePage.summary.data()
			});
			const auto& clients = RegisteredClients();
			const auto& pages = OrderedPages();
			const auto& actions = OrderedActions();
			const auto statuses =
				RollupClientStatuses(CurrentClientStatuses());
			const auto health = HostSubsystemHealthRegistry().Snapshots();
			const auto now = HealthClock::now();
			const auto attention =
				CountClientsNeedingAttention(clients, statuses);
			const auto summary =
				BuildHomeHealthSummary(health, attention, now);
			const auto severity = HealthStatusSeverity(
				HomeHealthSeverity(health, attention, now));

			DrawSectionHeader(
				"About",
				FindPhosphorIconGlyphOrZero("info"));
			{
				const Theme::FontGuard font{ Theme::FontRole::kSubtext };
				ImGui::TextWrapped("%s", HomeAboutText().data());
			}
			ImGui::Spacing();
			DrawSectionHeader(
				"Overview",
				FindPhosphorIconGlyphOrZero(kHostHomePage.iconName));
			char identity[128]{};
			std::snprintf(
				identity,
				sizeof(identity),
				"%.*s %.*s",
				static_cast<int>(kHostDisplayName.size()),
				kHostDisplayName.data(),
				static_cast<int>(kHostVersion.size()),
				kHostVersion.data());
			DrawBulletText(identity);
			char registry[128]{};
			std::snprintf(
				registry,
				sizeof(registry),
				"%zu mods | %zu pages | %zu actions",
				clients.size(),
				pages.size(),
				actions.size());
			DrawBulletText(registry);
			ImGui::PushStyleColor(ImGuiCol_Text, Theme::StatusTextColor(severity));
			DrawBulletText(summary.c_str());
			ImGui::PopStyleColor();

			ImGui::Spacing();
			DrawSectionHeader(
				"Quick Links",
				FindPhosphorIconGlyphOrZero("link"));
			std::vector<LinkRowEntry> quickLinks;
			for (const auto& link : HomeQuickLinks())
			{
				quickLinks.push_back({
					link.label,
					{
						DMUI_EXTERNAL_TARGET_URI,
						std::string{ link.url }
					},
					link.note,
					FindPhosphorIconGlyphOrZero(link.iconName),
					link.enabled,
					DMUI_LINK_ACTION_COPY_TARGET
				});
			}
			(void)DrawLinkRow("##DearModdingUI.HomeQuickLinks", quickLinks);

			ImGui::Spacing();
			DrawSectionHeader(
				"FAQ",
				FindPhosphorIconGlyphOrZero("question"));
			const auto faq = BuildHomeFaq(MenuToggleKeyName(
				HostSettings::MenuToggleVirtualKey()));
			std::vector<FaqRowEntry> entries;
			entries.reserve(faq.size());
			for (const auto& entry : faq)
				entries.push_back({ entry.question, entry.answer });
			DrawFaq("##DearModdingUI.HomeFaq", entries);
		}

		void DrawHealth(HostPageViewState& a_state) noexcept
		{
			const auto& clients = RegisteredClients();
			const auto& pages = OrderedPages();
			const auto& actions = OrderedActions();
			const auto statuses =
				RollupClientStatuses(CurrentClientStatuses());
			const auto health = HostSubsystemHealthRegistry().Snapshots();
			const auto now = HealthClock::now();
			const auto diagnostics = CurrentClientDiagnostics();
			constexpr auto extentPolicy =
				TitleRowButtonExtentPolicy::kTitleBar;
			const auto extent = ResolveTitleRowButtonExtent(
				extentPolicy,
				ImGui::GetFontSize(),
				TitleBarButtonPadding());
			const std::array buttons{
				TitleRowButton{
					"##DearModdingUI.CopyHealthReport",
					extent,
					FindPhosphorIconGlyphOrZero("clipboard-text"),
					"Copy report",
					"Copy a diagnostics report to the clipboard."
				}
			};
			if (DrawTitleRow({
					.title = kHostHealthPage.displayName.data(),
					.titleScale = Theme::kFeatureTitleScale,
					.buttons = buttons,
					.buttonExtentPolicy = extentPolicy,
					.summary = kHostHealthPage.summary.data()
				}))
			{
				const auto report = BuildHealthDiagnosticsReport(
					kHostDisplayName,
					kHostVersion,
					health,
					clients,
					statuses,
					diagnostics,
					now);
				ImGui::SetClipboardText(report.c_str());
			}

			DrawSectionHeader(
				"Host subsystems",
				FindPhosphorIconGlyphOrZero(kHostHealthPage.iconName));
			const auto subsystemRows = BuildHealthSubsystemRows(health, now);
			if (subsystemRows.empty())
				DrawBulletText("No host subsystem observations are available.");
			else if (const auto table = SettingsTable::Begin(
					DMUI_INVALID_CLIENT_HANDLE,
					"##DearModdingUI.HostHealthSubsystems");
				table.result == DMUI_RESULT_OK && table.visible)
			{
				for (const auto& row : subsystemRows)
				{
					const auto id = "Subsystem/" + row.identity;
					DrawHostDetailRow(
						id.c_str(),
						row.identity.c_str(),
						row.reason.c_str(),
						[&]() noexcept {
							ImGui::TextColored(
								Theme::StatusTextColor(
									HealthStatusSeverity(row.severity)),
								"Status: %s | %s in state",
								row.stateLabel.c_str(),
								row.durationLabel.c_str());
						});
				}
				(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
			}

			const auto sections = BuildHealthClientSections(clients);
			for (size_t sectionIndex = 0;
				sectionIndex < sections.size();
				++sectionIndex)
			{
				ImGui::Spacing();
				const auto& section = sections[sectionIndex];
				DrawSectionHeader(section.heading.c_str(), section.glyph);
				if (section.clients.empty())
				{
					DrawBulletText("No client mods registered this session.");
					continue;
				}
				ImGui::PushID(static_cast<int>(sectionIndex));
				const auto table = SettingsTable::Begin(
					DMUI_INVALID_CLIENT_HANDLE,
					"##DearModdingUI.HostHealthClients");
				if (table.result == DMUI_RESULT_OK && table.visible)
				{
					for (const auto* client : section.clients)
					{
						const auto pageCount = std::ranges::count(
							pages,
							client->handle,
							&RegisteredPage::client);
						const auto actionCount = std::ranges::count(
							actions,
							client->handle,
							&RegisteredAction::client);
						const auto description = std::format(
							"{} | {} pages | {} actions",
							client->id,
							pageCount,
							actionCount);
						const auto version = std::format(
							"Version {}.{}",
							client->version >> 16,
							client->version & 0xFFFFu);
						const auto* status =
							FindClientStatus(statuses, client->handle);
						const auto id = "Client/" + client->id;
						DrawHostDetailRow(
							id.c_str(),
							client->displayName.c_str(),
							description.c_str(),
							[&]() noexcept {
								ImGui::TextUnformatted(version.c_str());
								ImGui::SameLine();
								ImGui::TextColored(
									Theme::StatusTextColor(
										EffectiveClientStatusSeverity(
											client->callbackFailed,
											status)),
									"Status: %s",
									ClientStatusLabel(
										client->callbackFailed,
										status));
							});
					}
					(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
				}
				ImGui::PopID();
			}

			ImGui::Spacing();
			DrawSectionHeader(
				"Reported problems",
				FindPhosphorIconGlyphOrZero("warning-circle"));
			const auto diagnosticSections =
				BuildHealthDiagnosticSections(clients, diagnostics);
			if (diagnosticSections.empty())
			{
				DrawBulletText("No client diagnostics have been reported.");
				return;
			}
			ImGui::Indent();
			for (const auto& section : diagnosticSections)
			{
				auto expanded = a_state.diagnosticExpansion.try_emplace(
					section.clientId,
					false).first;
				const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
				{
					const Theme::FontGuard font{ Theme::FontRole::kBody };
					(void)DrawSelectableRow({
						.id = section.clientId.c_str(),
						.label = section.disclosureLabel.c_str(),
						.leadingAffordance = RowLeadingAffordance::kArrow,
						.expanded = &expanded->second,
						.textColor = textColor,
						.hoveredTextColor = textColor,
						.highlightStyle = RowHighlightStyle::kRoundedFill,
						.clickBehavior = RowClickBehavior::kToggle
					});
				}
				if (!expanded->second)
					continue;
				ImGui::Indent();
				ImGui::PushID(section.clientId.c_str());
				const auto table = SettingsTable::Begin(
					DMUI_INVALID_CLIENT_HANDLE,
					"##DearModdingUI.HostReportedProblems");
				if (table.result == DMUI_RESULT_OK && table.visible)
				{
					for (size_t index = 0; index < section.rows.size(); ++index)
					{
						const auto& row = section.rows[index];
						const auto id = std::format("Diagnostic/{}", index);
						DrawHostDetailRow(
							id.c_str(),
							row.summary.c_str(),
							row.description.c_str(),
							[&]() noexcept {
								ImGui::TextColored(
									Theme::StatusTextColor(row.severity),
									"%s",
									row.occurrenceLabel.c_str());
							});
					}
					(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
				}
				if (!section.droppedReportLabel.empty())
					ImGui::TextDisabled(
						"%s",
						section.droppedReportLabel.c_str());
				ImGui::PopID();
				ImGui::Unindent();
				ImGui::Spacing();
			}
			ImGui::Unindent();
		}

		void DrawSettings() noexcept
		{
			constexpr auto extentPolicy =
				TitleRowButtonExtentPolicy::kTitleBar;
			const auto extent = ResolveTitleRowButtonExtent(
				extentPolicy,
				ImGui::GetFontSize(),
				TitleBarButtonPadding());
			const std::array actions{
				HostSettingsTitleButton{
					SettingsAction::kReset,
					"##DearModdingUI.HostSettingsResetButton",
					"Reset",
					"Reset saves the default sidebar layout immediately and "
					"loads other shipped interface defaults into the draft. "
					"Use Apply to save those.",
					SettingsActionButtonWidth(
						SettingsAction::kReset,
						"Reset",
						extent) },
				HostSettingsTitleButton{
					SettingsAction::kRevert,
					"##DearModdingUI.HostSettingsRevertButton",
					"Revert",
					"Revert discards pending interface edits and restores "
					"saved settings. Sidebar layout changes are already saved.",
					SettingsActionButtonWidth(
						SettingsAction::kRevert,
						"Revert",
						extent) },
				HostSettingsTitleButton{
					SettingsAction::kApply,
					"##DearModdingUI.HostSettingsApplyButton",
					"Apply",
					"Apply saves host settings to DearModdingUI.toml. "
					"Sidebar layout changes save immediately; appearance "
					"previews update live, and typography rebuilds once if "
					"needed.",
					SettingsActionButtonWidth(
						SettingsAction::kApply,
						"Apply",
						extent) }
			};
			const auto dirty =
				HostSettingsTitleActionEnabled(SettingsAction::kApply);
			std::array<TitleRowButton, actions.size()> buttons{};
			for (size_t index = 0; index < actions.size(); ++index)
			{
				const auto& action = actions[index];
				const auto presentation =
					ResolveSettingsActionButtonPresentation(
						action.action,
						HasIconGlyph(SettingsActionGlyph(action.action)));
				buttons[index] = {
					action.id,
					action.width,
					presentation.glyph,
					action.label,
					action.tooltip,
					SettingsActionEnabled(action.action, dirty)
				};
			}
			const auto pressed = DrawTitleRow({
				.title = kHostSettingsPage.displayName.data(),
				.titleScale = Theme::kFeatureTitleScale,
				.buttons = buttons,
				.buttonExtentPolicy = extentPolicy,
				.summary = kHostSettingsPage.summary.data()
			});
			if (pressed)
				InvokeHostSettingsTitleAction(actions[*pressed].action);
			DrawHostSettingsControls();
		}
	}

	void DrawHostPage(
		HostPageKind a_page,
		HostPageViewState& a_state) noexcept
	{
		switch (a_page)
		{
		case HostPageKind::kHealth:
			DrawHealth(a_state);
			break;
		case HostPageKind::kSettings:
			DrawSettings();
			break;
		default:
			DrawHome();
			break;
		}
	}
}
