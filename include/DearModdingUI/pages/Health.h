#pragma once

#include <DearModdingUI/host/Diagnostics.h>
#include <DearModdingUI/host/Status.h>
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
		HealthSeverity severity{ HealthSeverity::kNeutral };
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
		std::vector<HealthDiagnosticRow> rows;
		size_t droppedReportCount{};
		std::string droppedReportLabel;
	};

	[[nodiscard]] constexpr DMUI_StatusSeverity HealthStatusSeverity(
		HealthSeverity a_severity) noexcept
	{
		switch (a_severity)
		{
		case HealthSeverity::kSuccess:
			return DMUI_STATUS_SEVERITY_SUCCESS;
		case HealthSeverity::kWarning:
			return DMUI_STATUS_SEVERITY_WARNING;
		case HealthSeverity::kError:
			return DMUI_STATUS_SEVERITY_ERROR;
		default:
			return DMUI_STATUS_SEVERITY_INFO;
		}
	}

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
		std::span<const ClientDiagnosticSnapshot> a_diagnostics,
		HealthClock::time_point a_now = HealthClock::now());
}
