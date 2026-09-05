#pragma once

#include <DearModdingUI/Diagnostics.h>
#include <DearModdingUI/Status.h>
#include <Support/SubsystemHealth.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace DearModdingUI
{
	struct RegisteredClient;

	struct HealthClientSection
	{
		std::string heading;
		char32_t glyph{};
		std::vector<const RegisteredClient*> clients;
	};

	struct HealthSubsystemRow
	{
		std::string identity;
		HealthState state{ HealthState::kWaiting };
		std::string stateLabel;
		std::string durationLabel;
		std::string reason;
	};

	struct HealthDiagnosticRow
	{
		DMUI_StatusSeverity severity{ DMUI_STATUS_SEVERITY_INFO };
		std::string scope;
		std::string summary;
		std::string detail;
		std::string description;
		std::string occurrenceLabel;
		uint64_t occurrenceCount{ 1 };
	};

	struct HealthDiagnosticSection
	{
		DMUI_ClientHandle client{ DMUI_INVALID_CLIENT_HANDLE };
		std::string clientId;
		std::string clientDisplayName;
		DMUI_StatusSeverity worstSeverity{ DMUI_STATUS_SEVERITY_INFO };
		std::string severitySummary;
		std::string disclosureLabel;
		bool defaultExpanded{ false };
		std::vector<HealthDiagnosticRow> rows;
		size_t droppedReportCount{};
		std::string droppedReportLabel;
	};

	[[nodiscard]] std::vector<HealthClientSection> BuildHealthClientSections(
		const std::vector<RegisteredClient>& a_clients);
	[[nodiscard]] std::vector<HealthSubsystemRow> BuildHealthSubsystemRows(
		std::span<const HealthSnapshot> a_snapshots,
		HealthClock::time_point a_now = HealthClock::now());
	[[nodiscard]] std::vector<HealthDiagnosticSection>
		BuildHealthDiagnosticSections(
			const std::vector<RegisteredClient>& a_clients,
			std::span<const ClientDiagnosticSnapshot> a_diagnostics);
	[[nodiscard]] std::string BuildHealthDiagnosticsReport(
		std::string_view a_hostName,
		std::string_view a_hostVersion,
		std::span<const HealthSnapshot> a_subsystems,
		const std::vector<RegisteredClient>& a_clients,
		std::span<const ClientStatus> a_statuses,
		std::span<const ClientDiagnosticSnapshot> a_diagnostics);
}
