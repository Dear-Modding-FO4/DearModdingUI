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
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include <DearModdingUI/host/Status.h>
#include <DearModdingUI/localization/Localization.h>
#include <DearModdingUI/presentation/Theme.h>

#include <Support/Revision.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ranges>
#include <vector>

namespace DearModdingUI
{
	namespace
	{
		constexpr Support::Phrase kSettingsResetText{ "$DMUI_Pages_SettingsReset", "Reset" };
		constexpr Support::Phrase kSettingsRevertText{ "$DMUI_Pages_SettingsRevert", "Revert" };

		[[nodiscard]] Support::Message ModCount(uint64_t a_count)
		{
			return Support::Message::Plural({ "$DMUI_Count_Mods", "{} mod", "{} mods" }, a_count, a_count);
		}

		[[nodiscard]] Support::Message PageCount(uint64_t a_count)
		{
			return Support::Message::Plural({ "$DMUI_Count_Pages", "{} page", "{} pages" }, a_count, a_count);
		}

		[[nodiscard]] Support::Message ActionCount(uint64_t a_count)
		{
			return Support::Message::Plural({ "$DMUI_Count_Actions", "{} action", "{} actions" }, a_count, a_count);
		}

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

		// Durations, deadlines, and transient statuses advance by the second.
		struct HostPageInputs
		{
			HostStateRevision host;
			uint64_t health{};
			uint64_t settings{};
			int64_t second{};

			[[nodiscard]] bool operator==(const HostPageInputs&) const noexcept = default;
		};

		[[nodiscard]] HostPageInputs CurrentHostPageInputs() noexcept
		{
			return {
				CurrentHostStateRevision(),
				HostSubsystemHealthRegistry().Revision(),
				HostSettings::Revision(),
				std::chrono::duration_cast<std::chrono::seconds>(
					HealthClock::now().time_since_epoch()).count()
			};
		}

		struct HomeModel
		{
			std::string registry;
			std::string summary;
			DMUI_StatusSeverity severity{ DMUI_STATUS_SEVERITY_INFO };
			std::vector<HomeFaqEntry> faq;
			// Views into faq; the model rebuilds in place.
			std::vector<FaqRowEntry> faqRows;
		};

		void BuildHome(HomeModel& a_model)
		{
			const auto& clients = RegisteredClients();
			const auto statuses =
				RollupClientStatuses(CurrentClientStatuses());
			const auto health = HostSubsystemHealthRegistry().Snapshots();
			const auto now = HealthClock::now();
			const auto attention =
				CountClientsNeedingAttention(clients, statuses);
			a_model.registry = Localization::Render(Support::Message::Join(
				{ ModCount(clients.size()),
					PageCount(OrderedPages().size()),
					ActionCount(OrderedActions().size()) },
				" | "));
			a_model.summary = BuildHomeHealthSummary(health, attention, now);
			a_model.severity = HealthStatusSeverity(
				HomeHealthSeverity(health, attention, now));
			a_model.faq = BuildHomeFaq(
				FormatHotkeyChord(HostSettings::Current().menuToggleKey));
			a_model.faqRows.clear();
			for (const auto& entry : a_model.faq)
				a_model.faqRows.push_back({ entry.question, entry.answer });
		}

		[[nodiscard]] const std::vector<LinkRowEntry>& HomeQuickLinkRows()
		{
			static const auto rows = [] {
				std::vector<LinkRowEntry> result;
				for (const auto& link : HomeQuickLinks())
				{
					result.push_back({
						link.label,
						{
							DMUI_EXTERNAL_TARGET_URI,
							std::string{ link.url }
						},
						link.note,
						FindPhosphorSlugGlyphOrZero(link.iconName),
						link.enabled,
						DMUI_LINK_ACTION_COPY_TARGET
					});
				}
				return result;
			}();
			return rows;
		}

		void DrawHome() noexcept
		{
			static Support::Derived<HostPageInputs, HomeModel> cache;
			const auto& model = cache.Get(CurrentHostPageInputs(), &BuildHome);
			(void)DrawTitleRow({
				.title = HostPageName(HostPageKind::kHome),
				.titleScale = Theme::kFeatureTitleScale,
				.summary = HostPageSummary(HostPageKind::kHome)
			});

			DrawSectionHeader(
				Localization::Text("$DMUI_Home_AboutHeader", "About"),
				FindPhosphorSlugGlyphOrZero("info"));
			{
				const Theme::FontGuard font{ Theme::FontRole::kSubtext };
				ImGui::TextWrapped("%s", HomeAboutText().data());
			}
			Theme::SectionSpacing();
			DrawSectionHeader(
				Localization::Text("$DMUI_Home_OverviewHeader", "Overview"),
				FindPhosphorSlugGlyphOrZero(kHostHomePage.iconName));
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
			DrawBulletText(model.registry.c_str());
			ImGui::PushStyleColor(ImGuiCol_Text, Theme::StatusTextColor(model.severity));
			DrawBulletText(model.summary.c_str());
			ImGui::PopStyleColor();

			Theme::SectionSpacing();
			DrawSectionHeader(
				Localization::Text("$DMUI_Home_QuickLinksHeader", "Quick Links"),
				FindPhosphorSlugGlyphOrZero("link"));
			(void)DrawLinkRow("##DearModdingUI.HomeQuickLinks", HomeQuickLinkRows());

			Theme::SectionSpacing();
			DrawSectionHeader(
				Localization::Text("$DMUI_Home_FaqHeader", "FAQ"),
				PhosphorGlyph::kQuestion);
			DrawFaq("##DearModdingUI.HomeFaq", model.faqRows);
		}

		struct HealthSubsystemView
		{
			std::string id;
			HealthSubsystemRow row;
			std::string status;
		};

		struct HealthClientView
		{
			const RegisteredClient* client{};
			std::string id;
			std::string description;
			std::string version;
			std::string status;
			DMUI_StatusSeverity severity{ DMUI_STATUS_SEVERITY_INFO };
		};

		struct HealthClientGroup
		{
			std::string heading;
			char32_t glyph{};
			std::vector<HealthClientView> clients;
		};

		struct HealthModel
		{
			std::vector<HealthSubsystemView> subsystems;
			std::vector<HealthClientGroup> clientGroups;
			std::vector<HealthDiagnosticSection> diagnostics;
		};

		void BuildHealth(HealthModel& a_model)
		{
			const auto& clients = RegisteredClients();
			const auto& pages = OrderedPages();
			const auto& actions = OrderedActions();
			const auto statuses =
				RollupClientStatuses(CurrentClientStatuses());
			const auto now = HealthClock::now();

			a_model.subsystems.clear();
			for (auto& row : BuildHealthSubsystemRows(
					HostSubsystemHealthRegistry().Snapshots(), now))
			{
				auto id = "Subsystem/" + row.identity;
				auto status = Localization::Format(
					"$DMUI_Health_SubsystemStatus",
					"Status: {} | {} in state",
					row.stateLabel,
					row.durationLabel);
				a_model.subsystems.push_back({ std::move(id), std::move(row), std::move(status) });
			}

			a_model.clientGroups.clear();
			for (auto& section : BuildHealthClientSections(clients))
			{
				auto& group = a_model.clientGroups.emplace_back(
					std::move(section.heading), section.glyph);
				for (const auto* client : section.clients)
				{
					const auto* status = FindClientStatus(statuses, client->handle);
					group.clients.push_back({
						client,
						"Client/" + client->id,
						Localization::Render(Support::Message::Join(
							{ Support::Message::Text(client->id),
								PageCount(static_cast<uint64_t>(
									std::ranges::count(pages, client->handle, &RegisteredPage::client))),
								ActionCount(static_cast<uint64_t>(
									std::ranges::count(actions, client->handle, &RegisteredAction::client))) },
							" | ")),
						Localization::Format(
							"$DMUI_Health_ClientVersion",
							"Version {}.{}",
							client->version >> 16,
							client->version & 0xFFFFu),
						Localization::Format(
							"$DMUI_Health_ClientStatus",
							"Status: {}",
							ClientStatusLabel(client->callbackFailed, status)),
						EffectiveClientStatusSeverity(client->callbackFailed, status)
					});
				}
			}

			a_model.diagnostics =
				BuildHealthDiagnosticSections(clients, CurrentClientDiagnostics());
		}

		void CopyHealthReport()
		{
			const auto report = BuildHealthDiagnosticsReport(
				kHostDisplayName,
				kHostVersion,
				HostSubsystemHealthRegistry().Snapshots(),
				RegisteredClients(),
				RollupClientStatuses(CurrentClientStatuses()),
				CurrentClientDiagnostics(),
				HealthClock::now());
			ImGui::SetClipboardText(report.c_str());
		}

		void DrawHealth(HostPageViewState& a_state) noexcept
		{
			static Support::Derived<HostPageInputs, HealthModel> cache;
			const auto& model = cache.Get(CurrentHostPageInputs(), &BuildHealth);
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
					PhosphorGlyph::kClipboardText,
					Localization::Text("$DMUI_Health_CopyReport", "Copy report"),
					Localization::Text(
						"$DMUI_Health_CopyReportTooltip",
						"Copy a diagnostics report to the clipboard.")
				}
			};
			if (DrawTitleRow({
					.title = HostPageName(HostPageKind::kHealth),
					.titleScale = Theme::kFeatureTitleScale,
					.buttons = buttons,
					.buttonExtentPolicy = extentPolicy,
					.summary = HostPageSummary(HostPageKind::kHealth)
				}))
				CopyHealthReport();

			DrawSectionHeader(
				Localization::Text("$DMUI_Health_HostSubsystemsHeader", "Host subsystems"),
				FindPhosphorSlugGlyphOrZero(kHostHealthPage.iconName));
			if (model.subsystems.empty())
				DrawBulletText(Localization::Text(
					"$DMUI_Health_NoSubsystemObservations",
					"No host subsystem observations are available."));
			else if (const auto table = SettingsTable::Begin(
					DMUI_INVALID_CLIENT_HANDLE,
					"##DearModdingUI.HostHealthSubsystems");
				table.result == DMUI_RESULT_OK && table.visible)
			{
				for (const auto& subsystem : model.subsystems)
				{
					DrawHostDetailRow(
						subsystem.id.c_str(),
						subsystem.row.displayName.c_str(),
						subsystem.row.reason.c_str(),
						[&]() noexcept {
							ImGui::TextColored(
								Theme::StatusTextColor(
									HealthStatusSeverity(subsystem.row.severity)),
								"%s",
								subsystem.status.c_str());
						});
				}
				(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
			}

			for (size_t sectionIndex = 0;
				sectionIndex < model.clientGroups.size();
				++sectionIndex)
			{
				Theme::SectionSpacing();
				const auto& section = model.clientGroups[sectionIndex];
				DrawSectionHeader(section.heading.c_str(), section.glyph);
				if (section.clients.empty())
				{
					DrawBulletText(Localization::Text(
					"$DMUI_Health_NoClientMods",
					"No client mods registered this session."));
					continue;
				}
				ImGui::PushID(static_cast<int>(sectionIndex));
				const auto table = SettingsTable::Begin(
					DMUI_INVALID_CLIENT_HANDLE,
					"##DearModdingUI.HostHealthClients");
				if (table.result == DMUI_RESULT_OK && table.visible)
				{
					for (const auto& row : section.clients)
					{
						DrawHostDetailRow(
							row.id.c_str(),
							row.client->displayName.c_str(),
							row.description.c_str(),
							[&]() noexcept {
								ImGui::TextUnformatted(row.version.c_str());
								ImGui::SameLine();
								ImGui::TextColored(
									Theme::StatusTextColor(row.severity),
									"%s",
									row.status.c_str());
							});
					}
					(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
				}
				ImGui::PopID();
			}

			Theme::SectionSpacing();
			DrawSectionHeader(
				Localization::Text("$DMUI_Health_ReportedProblemsHeader", "Reported problems"),
				FindPhosphorSlugGlyphOrZero("warning-circle"));
			const auto& diagnosticSections = model.diagnostics;
			if (diagnosticSections.empty())
			{
				DrawBulletText(Localization::Text(
				"$DMUI_Health_NoDiagnostics",
				"No client diagnostics have been reported."));
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
						.expanded = &expanded->second,
						.textColor = textColor,
						.hoveredTextColor = textColor,
						.highlightStyle = RowHighlightStyle::kRoundedFill
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
						char id[32]{};
						std::snprintf(id, sizeof(id), "Diagnostic/%zu", index);
						DrawHostDetailRow(
							id,
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
					Localization::Text(kSettingsResetText),
					Localization::Text(
						"$DMUI_Pages_SettingsResetTooltip",
						"Reset saves the default sidebar layout immediately and "
						"loads other shipped interface defaults into the draft. "
						"Use Apply to save those."),
					SettingsActionButtonWidth(
						SettingsAction::kReset,
						Localization::Text(kSettingsResetText),
						extent) },
				HostSettingsTitleButton{
					SettingsAction::kRevert,
					"##DearModdingUI.HostSettingsRevertButton",
					Localization::Text(kSettingsRevertText),
					Localization::Text(
						"$DMUI_Pages_SettingsRevertTooltip",
						"Revert discards pending interface edits and restores "
						"saved settings. Sidebar layout changes are already saved."),
					SettingsActionButtonWidth(
						SettingsAction::kRevert,
						Localization::Text(kSettingsRevertText),
						extent) },
				HostSettingsTitleButton{
					SettingsAction::kApply,
					"##DearModdingUI.HostSettingsApplyButton",
					Localization::Text(kSettingsApplyControl),
					Localization::Text(
						"$DMUI_Pages_SettingsApplyTooltip",
						"Apply saves host settings to DearModdingUI.toml. "
						"Sidebar layout changes save immediately; appearance "
						"previews update live, and typography rebuilds once if "
						"needed."),
					SettingsActionButtonWidth(
						SettingsAction::kApply,
						Localization::Text(kSettingsApplyControl),
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
				.title = HostPageName(HostPageKind::kSettings),
				.titleScale = Theme::kFeatureTitleScale,
				.buttons = buttons,
				.buttonExtentPolicy = extentPolicy,
				.summary = HostPageSummary(HostPageKind::kSettings)
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
