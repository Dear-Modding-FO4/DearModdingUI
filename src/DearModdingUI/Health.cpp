#include <DearModdingUI/Health.h>

#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/Registry.h>

#include <algorithm>
#include <cstdio>
#include <format>
#include <map>
#include <memory>
#include <ranges>
#include <string_view>
#include <tuple>
#include <utility>

namespace DearModdingUI
{
	namespace
	{
		[[nodiscard]] const char* HealthStateLabel(HealthState a_state) noexcept
		{
			switch (a_state)
			{
			case HealthState::kWaiting:
				return "Waiting";
			case HealthState::kProgressing:
				return "Progressing";
			case HealthState::kReady:
				return "Ready";
			default:
				return "Unknown";
			}
		}

		[[nodiscard]] const char* StatusSeverityLabel(
			DMUI_StatusSeverity a_severity) noexcept
		{
			switch (a_severity)
			{
			case DMUI_STATUS_SEVERITY_INFO:
				return "Info";
			case DMUI_STATUS_SEVERITY_SUCCESS:
				return "Success";
			case DMUI_STATUS_SEVERITY_WARNING:
				return "Warning";
			case DMUI_STATUS_SEVERITY_ERROR:
				return "Error";
			default:
				return "Unknown";
			}
		}

		[[nodiscard]] uint32_t DiagnosticSeverityRank(
			DMUI_StatusSeverity a_severity) noexcept
		{
			switch (a_severity)
			{
			case DMUI_STATUS_SEVERITY_ERROR:
				return 0;
			case DMUI_STATUS_SEVERITY_WARNING:
				return 1;
			case DMUI_STATUS_SEVERITY_SUCCESS:
				return 2;
			default:
				return 3;
			}
		}

		[[nodiscard]] std::string DiagnosticSeveritySummary(
			uint64_t a_errors,
			uint64_t a_warnings,
			uint64_t a_successes,
			uint64_t a_info)
		{
			std::string summary;
			const auto append =
				[&](uint64_t a_count,
					std::string_view a_singular,
					std::string_view a_plural) {
					if (a_count == 0)
						return;
					if (!summary.empty())
						summary.append(", ");
					summary.append(std::to_string(a_count));
					summary.push_back(' ');
					summary.append(a_count == 1 ?
						a_singular :
						a_plural);
				};
			append(a_errors, "error", "errors");
			append(a_warnings, "warning", "warnings");
			append(a_successes, "success", "successes");
			append(a_info, "info", "info");
			return summary;
		}

		[[nodiscard]] const ClientStatus* FindStatus(
			std::span<const ClientStatus> a_statuses,
			DMUI_ClientHandle a_client) noexcept
		{
			const auto status = std::ranges::find(
				a_statuses,
				a_client,
				&ClientStatus::client);
			return status == a_statuses.end() ?
				nullptr :
				std::to_address(status);
		}

		[[nodiscard]] const RegisteredClient* FindClient(
			const std::vector<RegisteredClient>& a_clients,
			DMUI_ClientHandle a_client) noexcept
		{
			const auto client = std::ranges::find(
				a_clients,
				a_client,
				&RegisteredClient::handle);
			return client == a_clients.end() ?
				nullptr :
				std::to_address(client);
		}

		[[nodiscard]] std::string DroppedReportLabel(size_t a_count)
		{
			return std::format(
				"{} further diagnostic report{} {} not retained.",
				a_count,
				a_count == 1 ? "" : "s",
				a_count == 1 ? "was" : "were");
		}

		[[nodiscard]] std::string DiagnosticDescription(
			std::string_view a_scope,
			std::string_view a_detail)
		{
			std::string description;
			if (!a_scope.empty())
			{
				description.append("Scope: ");
				description.append(a_scope);
			}
			if (!a_detail.empty())
			{
				if (!description.empty())
					description.push_back('\n');
				description.append(a_detail);
			}
			return description;
		}

		[[nodiscard]] std::string FormatHealthDuration(
			HealthClock::duration a_duration)
		{
			const auto seconds = (std::max)(
				int64_t{},
				std::chrono::duration_cast<std::chrono::seconds>(
					a_duration).count());
			char label[64]{};
			if (seconds < 60)
			{
				std::snprintf(label, sizeof(label), "%llds",
					static_cast<long long>(seconds));
			}
			else if (seconds < 3600)
			{
				std::snprintf(
					label,
					sizeof(label),
					"%lldm %llds",
					static_cast<long long>(seconds / 60),
					static_cast<long long>(seconds % 60));
			}
			else
			{
				std::snprintf(
					label,
					sizeof(label),
					"%lldh %lldm",
					static_cast<long long>(seconds / 3600),
					static_cast<long long>((seconds % 3600) / 60));
			}
			return label;
		}
	}

	std::vector<HealthClientSection> BuildHealthClientSections(
		const std::vector<RegisteredClient>& a_clients)
	{
		std::vector<const RegisteredClient*> sortedClients;
		sortedClients.reserve(a_clients.size());
		for (const auto& client : a_clients)
			sortedClients.push_back(&client);
		std::ranges::sort(
			sortedClients,
			[](const auto* a_left, const auto* a_right) {
				return std::tie(a_left->displayName, a_left->id) <
					std::tie(a_right->displayName, a_right->id);
			});

		HealthClientSection native{
			"Registered mods",
			PhosphorGlyph::kPuzzlePiece,
			{}
		};
		std::map<std::string, std::vector<const RegisteredClient*>> bridged;
		for (const auto* client : sortedClients)
		{
			if (client->origin == DMUI_CLIENT_ORIGIN_NATIVE)
				native.clients.push_back(client);
			else
				bridged[client->bridgeSourceLabel].push_back(client);
		}

		std::vector<HealthClientSection> sections;
		if (!native.clients.empty() || bridged.empty())
			sections.push_back(std::move(native));
		const auto bridgeGlyph = FindPhosphorIconGlyphOrZero("share-network");
		for (auto& [sourceLabel, clients] : bridged)
		{
			sections.push_back({
				sourceLabel.empty() ?
					"Bridged mods" :
					sourceLabel + " mods",
				bridgeGlyph,
				std::move(clients)
			});
		}
		return sections;
	}

	std::vector<HealthSubsystemRow> BuildHealthSubsystemRows(
		std::span<const HealthSnapshot> a_snapshots,
		HealthClock::time_point a_now)
	{
		std::vector<HealthSubsystemRow> rows;
		rows.reserve(a_snapshots.size());
		for (const auto& snapshot : a_snapshots)
		{
			rows.push_back({
				std::string{ snapshot.identity },
				snapshot.state,
				HealthStateLabel(snapshot.state),
				FormatHealthDuration(a_now - snapshot.enteredAt),
				std::string{ snapshot.reason }
			});
		}
		std::ranges::sort(
			rows,
			{},
			&HealthSubsystemRow::identity);
		return rows;
	}

	std::vector<HealthDiagnosticSection> BuildHealthDiagnosticSections(
		const std::vector<RegisteredClient>& a_clients,
		std::span<const ClientDiagnosticSnapshot> a_diagnostics)
	{
		std::vector<HealthDiagnosticSection> sections;
		sections.reserve(a_diagnostics.size());
		for (const auto& diagnostic : a_diagnostics)
		{
			if (diagnostic.records.empty() &&
				diagnostic.droppedReportCount == 0)
				continue;
			const auto* client = FindClient(a_clients, diagnostic.client);
			if (!client)
				continue;

			HealthDiagnosticSection section{
				.client = diagnostic.client,
				.clientId = client->id,
				.clientDisplayName = client->displayName,
				.worstSeverity = DMUI_STATUS_SEVERITY_INFO,
				.droppedReportCount = diagnostic.droppedReportCount,
				.droppedReportLabel = diagnostic.droppedReportCount == 0 ?
					std::string{} :
					DroppedReportLabel(
						diagnostic.droppedReportCount)
			};
			section.rows.reserve(diagnostic.records.size());
			uint64_t errors{};
			uint64_t warnings{};
			uint64_t successes{};
			uint64_t info{};
			for (const auto& record : diagnostic.records)
			{
				switch (record.severity)
				{
				case DMUI_STATUS_SEVERITY_ERROR:
					errors += record.occurrenceCount;
					break;
				case DMUI_STATUS_SEVERITY_WARNING:
					warnings += record.occurrenceCount;
					break;
				case DMUI_STATUS_SEVERITY_SUCCESS:
					successes += record.occurrenceCount;
					break;
				default:
					info += record.occurrenceCount;
					break;
				}
				section.rows.push_back({
					record.severity,
					record.scope,
					record.summary,
					record.detail,
					DiagnosticDescription(record.scope, record.detail),
					std::format(
						"{} \xC3\x97{}",
						StatusSeverityLabel(record.severity),
						record.occurrenceCount),
					record.occurrenceCount
				});
			}
			section.worstSeverity = errors != 0 ?
				DMUI_STATUS_SEVERITY_ERROR :
				(warnings != 0 ?
					DMUI_STATUS_SEVERITY_WARNING :
					(successes != 0 ?
						DMUI_STATUS_SEVERITY_SUCCESS :
						DMUI_STATUS_SEVERITY_INFO));
			section.severitySummary = DiagnosticSeveritySummary(
				errors,
				warnings,
				successes,
				info);
			section.disclosureLabel = section.clientDisplayName;
			if (!section.severitySummary.empty())
			{
				section.disclosureLabel.append(" \xE2\x80\x94 ");
				section.disclosureLabel.append(
					section.severitySummary);
			}
			std::ranges::sort(
				section.rows,
				[](const HealthDiagnosticRow& a_left,
					const HealthDiagnosticRow& a_right) {
					return std::tuple{
						static_cast<uint32_t>(DMUI_STATUS_SEVERITY_ERROR -
							a_left.severity),
						a_left.scope,
						a_left.summary
					} < std::tuple{
						static_cast<uint32_t>(DMUI_STATUS_SEVERITY_ERROR -
							a_right.severity),
						a_right.scope,
						a_right.summary
					};
				});
			sections.push_back(std::move(section));
		}
		std::ranges::sort(
			sections,
			[](const HealthDiagnosticSection& a_left,
				const HealthDiagnosticSection& a_right) {
				return std::tuple{
					DiagnosticSeverityRank(a_left.worstSeverity),
					a_left.clientDisplayName,
					a_left.clientId
				} < std::tuple{
					DiagnosticSeverityRank(a_right.worstSeverity),
					a_right.clientDisplayName,
					a_right.clientId
				};
			});
		return sections;
	}

	std::string BuildHealthDiagnosticsReport(
		std::string_view a_hostName,
		std::string_view a_hostVersion,
		std::span<const HealthSnapshot> a_subsystems,
		const std::vector<RegisteredClient>& a_clients,
		std::span<const ClientStatus> a_statuses,
		std::span<const ClientDiagnosticSnapshot> a_diagnostics)
	{
		std::string report;
		report.append("DearModdingUI diagnostics report\n\nHost: ");
		report.append(a_hostName);
		report.push_back(' ');
		report.append(a_hostVersion);
		report.append("\n\nHost subsystems\n");

		std::vector<const HealthSnapshot*> subsystems;
		subsystems.reserve(a_subsystems.size());
		for (const auto& subsystem : a_subsystems)
			subsystems.push_back(&subsystem);
		std::ranges::sort(
			subsystems,
			{},
			[](const HealthSnapshot* a_subsystem) {
				return a_subsystem->identity;
			});
		if (subsystems.empty())
		{
			report.append("- No observations\n");
		}
		else
		{
			for (const auto* subsystem : subsystems)
			{
				report.append("- ");
				report.append(subsystem->identity);
				report.append(": ");
				report.append(HealthStateLabel(subsystem->state));
				if (!subsystem->reason.empty())
				{
					report.append(" - ");
					report.append(subsystem->reason);
				}
				report.push_back('\n');
			}
		}

		report.append("\nRegistered mods\n");
		std::vector<const RegisteredClient*> clients;
		clients.reserve(a_clients.size());
		for (const auto& client : a_clients)
			clients.push_back(&client);
		std::ranges::sort(
			clients,
			[](const RegisteredClient* a_left,
				const RegisteredClient* a_right) {
				return std::tie(a_left->displayName, a_left->id) <
					std::tie(a_right->displayName, a_right->id);
			});
		if (clients.empty())
		{
			report.append("- None\n");
		}
		else
		{
			for (const auto* client : clients)
			{
				const auto* status = FindStatus(a_statuses, client->handle);
				report.append(std::format(
					"- {} {}.{} [{}]\n",
					client->displayName,
					client->version >> 16,
					client->version & 0xFFFFu,
					client->callbackFailed ?
						"Unavailable" :
						StatusSeverityLabel(
							status ?
								status->severity :
								DMUI_STATUS_SEVERITY_SUCCESS)));
			}
		}

		report.append("\nReported diagnostics\n");
		const auto sections =
			BuildHealthDiagnosticSections(a_clients, a_diagnostics);
		if (sections.empty())
		{
			report.append("- None\n");
		}
		else
		{
			for (const auto& section : sections)
			{
				report.append(section.clientDisplayName);
				report.append(" (");
				report.append(section.clientId);
				report.append(")\n");
				for (const auto& row : section.rows)
				{
					report.append("- [");
					report.append(StatusSeverityLabel(row.severity));
					report.append("] ");
					if (!row.scope.empty())
					{
						report.append(row.scope);
						report.append(": ");
					}
					report.append(row.summary);
					report.append(std::format(
						" (x{})",
						row.occurrenceCount));
					report.push_back('\n');
					if (!row.detail.empty())
					{
						report.append("  ");
						report.append(row.detail);
						report.push_back('\n');
					}
				}
				if (section.droppedReportCount != 0)
				{
					report.append("- ");
					report.append(section.droppedReportLabel);
					report.push_back('\n');
				}
			}
		}
		return report;
	}
}
