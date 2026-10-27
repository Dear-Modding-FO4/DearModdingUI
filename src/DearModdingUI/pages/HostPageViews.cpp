#include <DearModdingUI/pages/HostPageViews.h>

#include <DearModdingUI/controls/Controls.h>
#include <DearModdingUI/controls/SettingsActions.h>
#include <DearModdingUI/controls/Faq.h>
#include <DearModdingUI/controls/Section.h>
#include <DearModdingUI/controls/TextSpans.h>
#include <DearModdingUI/pages/Health.h>
#include <DearModdingUI/pages/Home.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/settings/HostSettingsView.h>
#include <DearModdingUI/presentation/IconResolver.h>
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
		constexpr Localization::Phrase kSettingsApplyText{ "$DMUI_Pages_SettingsApply", "Apply" };
		constexpr Localization::Phrase kSettingsResetText{ "$DMUI_Pages_SettingsReset", "Reset" };
		constexpr Localization::Phrase kSettingsRevertText{ "$DMUI_Pages_SettingsRevert", "Revert" };

		template <class DrawValue>
		void DrawHostDetailRow(
			const char* a_id,
			const char* a_label,
			const char* a_description,
			DrawValue&& a_drawValue) noexcept
		{
			const auto result = SettingsTable::BeginField(
				DMUI_INVALID_CLIENT_HANDLE,
				a_id,
				a_label,
				a_description,
				SettingsTable::FieldLayout::kLabelValue);
			if (result.result != DMUI_RESULT_OK || !result.visible)
				return;
			a_drawValue();
			bool resetPressed{};
			(void)SettingsTable::EndField(
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
					return severity == DMUI_SEVERITY_WARNING ||
						severity == DMUI_SEVERITY_ERROR;
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
			DMUI_Severity severity{ DMUI_SEVERITY_INFO };
			std::vector<HomeFaqEntry> faq;
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
			a_model.registry = Localization::Format(
				"$DMUI_Home_RegistryCounts",
				"{} mods | {} pages | {} actions",
				clients.size(),
				OrderedPages().size(),
				ActionCount());
			a_model.summary = BuildHomeHealthSummary(health, attention, now);
			a_model.severity = HealthStatusSeverity(
				HomeHealthSeverity(health, attention, now));
			a_model.faq = BuildHomeFaq(
				FormatHotkeyChord(HostSettings::Current().menuToggleKey));
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
						dmui::FindPhosphorSlugGlyphOrZero(link.iconName),
						link.enabled,
						DMUI_LINK_ACTION_COPY_TARGET
					});
				}
				return result;
			}();
			return rows;
		}

		void DrawHome(HostPageViewState& a_state) noexcept
		{
			static Addictol::Support::Derived<HostPageInputs, HomeModel> cache;
			const auto& model = cache.Get(CurrentHostPageInputs(), &BuildHome);
			(void)DrawTitleRow({
				.title = HostPageName(HostPageKind::kHome),
				.titleScale = Theme::Current().chrome.featureTitleScale,
				.summary = HostPageSummary(HostPageKind::kHome)
			});

			DrawSection({
				.title = Localization::Text("$DMUI_Home_AboutHeader", "About"),
				.glyph = dmui::FindPhosphorSlugGlyphOrZero("info")
			});
			(void)DrawTextSpans(
				HomeAboutText(),
				dmui::TextStyle{}.Role(DMUI_FONT_ROLE_SUBTEXT),
				{ .wrap = true });
			Theme::SectionSpacing();
			DrawSection({
				.title = Localization::Text("$DMUI_Home_OverviewHeader", "Overview"),
				.glyph = dmui::FindPhosphorSlugGlyphOrZero(kHostHomePage.iconName)
			});
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
			DrawBulletText(model.registry);
			DrawBulletText(model.summary, Theme::StatusTone(model.severity));

			Theme::SectionSpacing();
			DrawSection({
				.title = Localization::Text("$DMUI_Home_QuickLinksHeader", "Quick Links"),
				.glyph = dmui::FindPhosphorSlugGlyphOrZero("link")
			});
			(void)DrawLinkRow("##DearModdingUI.HomeQuickLinks", HomeQuickLinkRows());

			Theme::SectionSpacing();
			DrawSection({
				.title = Localization::Text("$DMUI_Home_FaqHeader", "FAQ"),
				.glyph = dmui::PhosphorGlyph::kQuestion
			});
			if (a_state.faqExpansion.size() < model.faq.size())
				a_state.faqExpansion.resize(model.faq.size(), false);
			std::vector<FaqRowEntry> faqRows;
			faqRows.reserve(model.faq.size());
			for (size_t index = 0; index < model.faq.size(); ++index)
			{
				faqRows.push_back({
					model.faq[index].question,
					model.faq[index].answer,
					&a_state.faqExpansion[index]
				});
			}
			DrawFaq("##DearModdingUI.HomeFaq", faqRows);
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
			DMUI_Severity severity{ DMUI_SEVERITY_INFO };
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
						Localization::Format(
							"$DMUI_Health_ClientCounts",
							"{} | {} pages | {} actions",
							client->id,
							std::ranges::count(pages, client->handle, &RegisteredPage::client),
							ActionCount(client->handle)),
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
			static Addictol::Support::Derived<HostPageInputs, HealthModel> cache;
			const auto& model = cache.Get(CurrentHostPageInputs(), &BuildHealth);
			const std::array buttons{
				MakeIconButtonSpec(
					"##DearModdingUI.CopyHealthReport",
					dmui::PhosphorGlyph::kClipboardText,
					Localization::Text("$DMUI_Health_CopyReport", "Copy report"),
					Localization::Text(
						"$DMUI_Health_CopyReportTooltip",
						"Copy a diagnostics report to the clipboard."))
			};
			if (DrawTitleRow({
					.title = HostPageName(HostPageKind::kHealth),
					.titleScale = Theme::Current().chrome.featureTitleScale,
					.buttons = buttons,
					.summary = HostPageSummary(HostPageKind::kHealth)
				}))
				CopyHealthReport();

			DrawSection({
				.title = Localization::Text("$DMUI_Health_HostSubsystemsHeader", "Host subsystems"),
				.glyph = dmui::FindPhosphorSlugGlyphOrZero(kHostHealthPage.iconName)
			});
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
						subsystem.row.identity.c_str(),
						subsystem.row.reason.c_str(),
						[&]() noexcept {
							(void)DrawTextSpans(
								subsystem.status,
								dmui::TextStyle{}.Tone(Theme::StatusTone(
									HealthStatusSeverity(subsystem.row.severity))));
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
				DrawSection({
					.title = section.heading.c_str(),
					.glyph = section.glyph
				});
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
								(void)DrawTextSpans(
									row.status,
									dmui::TextStyle{}.Tone(Theme::StatusTone(row.severity)));
							});
					}
					(void)SettingsTable::End(DMUI_INVALID_CLIENT_HANDLE);
				}
				ImGui::PopID();
			}

			Theme::SectionSpacing();
			DrawSection({
				.title = Localization::Text("$DMUI_Health_ReportedProblemsHeader", "Reported problems"),
				.glyph = dmui::FindPhosphorSlugGlyphOrZero("warning-circle")
			});
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
						.leadingAffordance = RowLeadingAffordance::kArrow,
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
								(void)DrawTextSpans(
									row.occurrenceLabel,
									dmui::TextStyle{}.Tone(Theme::StatusTone(row.severity)));
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
			struct Action
			{
				SettingsAction action;
				const char* id;
				const char* label;
				const char* tooltip;
				dmui::ButtonStyle style;
			};
			const std::array actions{
				Action{
					SettingsAction::kReset,
					"##DearModdingUI.HostSettingsResetButton",
					Localization::Text(kSettingsResetText),
					Localization::Text(
						"$DMUI_Pages_SettingsResetTooltip",
						"Reset saves the default sidebar layout immediately and "
						"loads other shipped interface defaults into the draft. "
						"Use Apply to save those."),
					dmui::ButtonStyle::kDefault },
				Action{
					SettingsAction::kRevert,
					"##DearModdingUI.HostSettingsRevertButton",
					Localization::Text(kSettingsRevertText),
					Localization::Text(
						"$DMUI_Pages_SettingsRevertTooltip",
						"Revert discards pending interface edits and restores "
						"saved settings. Sidebar layout changes are already saved."),
					dmui::ButtonStyle::kDefault },
				Action{
					SettingsAction::kApply,
					"##DearModdingUI.HostSettingsApplyButton",
					Localization::Text(kSettingsApplyText),
					Localization::Text(
						"$DMUI_Pages_SettingsApplyTooltip",
						"Apply saves host settings to DearModdingUI.toml. "
						"Sidebar layout changes save immediately; appearance "
						"previews update live, and typography rebuilds once if "
						"needed."),
					dmui::ButtonStyle::kPrimary }
			};
			std::array<ButtonSpec, actions.size()> buttons{};
			for (size_t index = 0; index < actions.size(); ++index)
			{
				const auto& action = actions[index];
				buttons[index] = {
					.id = action.id,
					.label = action.label,
					.glyph = SettingsActionGlyph(action.action),
					.tooltip = action.tooltip,
					.enabled = HostSettingsTitleActionEnabled(action.action),
					.style = action.style
				};
			}
			const auto pressed = DrawTitleRow({
				.title = HostPageName(HostPageKind::kSettings),
				.titleScale = Theme::Current().chrome.featureTitleScale,
				.buttons = buttons,
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
			DrawHome(a_state);
			break;
		}
	}
}
