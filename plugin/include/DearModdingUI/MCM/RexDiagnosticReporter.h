#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/MCM/DiagnosticReporter.h>

#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dmui
{
	class Client;
}

namespace DearModdingUI::MCM
{
	class RexDiagnosticReporter final : public DiagnosticReporter
	{
	public:
		RexDiagnosticReporter(
			std::string a_mod,
			std::string a_clientId,
			std::span<const Diagnostic> a_pending = {});

		void Report(Diagnostic a_diagnostic) noexcept override;
		void AttachClient(dmui::Client& a_client) noexcept;
		void ReportSummary(
			DMUI_StatusSeverity a_severity,
			std::string_view a_scope,
			std::string_view a_summary,
			std::string_view a_detail = {}) noexcept;

	private:
		struct PendingDiagnostic
		{
			DMUI_StatusSeverity severity{ DMUI_STATUS_SEVERITY_INFO };
			std::string scope;
			std::string summary;
			std::string detail;
		};

		void Submit(const Diagnostic& a_diagnostic) noexcept;
		void Submit(
			DMUI_StatusSeverity a_severity,
			std::string_view a_scope,
			std::string_view a_summary,
			std::string_view a_detail) noexcept;
		void Send(
			dmui::Client& a_client,
			const PendingDiagnostic& a_diagnostic) noexcept;

		std::string mod_;
		std::string clientId_;
		std::mutex mutex_;
		dmui::Client* client_{};
		std::vector<PendingDiagnostic> pending_;
	};
}
