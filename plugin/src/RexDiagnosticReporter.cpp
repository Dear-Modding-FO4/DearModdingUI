#include <DearModdingUI/MCM/RexDiagnosticReporter.h>

#include <DearModdingUI/Client.h>

#include <REX/REX.h>

#include <exception>

namespace DearModdingUI::MCM
{
	using namespace std::literals;

	namespace
	{
		inline constexpr auto kDiagnosticLogTag{
			"[dmui.mcm.diagnostic]"sv
		};

		[[nodiscard]] DMUI_StatusSeverity ToStatusSeverity(
			DiagnosticSeverity a_severity) noexcept
		{
			return a_severity == DiagnosticSeverity::kWarning ?
				DMUI_STATUS_SEVERITY_WARNING :
				DMUI_STATUS_SEVERITY_ERROR;
		}
	}

	RexDiagnosticReporter::RexDiagnosticReporter(
		std::string a_mod,
		std::string a_clientId,
		std::span<const Diagnostic> a_pending) :
		mod_(std::move(a_mod)),
		clientId_(std::move(a_clientId))
	{
		pending_.reserve(a_pending.size());
		for (const auto& diagnostic : a_pending)
		{
			pending_.push_back({
				ToStatusSeverity(diagnostic.severity),
				diagnostic.source,
				diagnostic.message,
				diagnostic.location
			});
		}
	}

	void RexDiagnosticReporter::Report(Diagnostic a_diagnostic) noexcept
	{
		if (a_diagnostic.location.empty())
		{
			if (a_diagnostic.severity == DiagnosticSeverity::kWarning)
				REX::WARN("{} mod=\"{}\" client_id=\"{}\" {}: {}"sv,
					kDiagnosticLogTag, mod_, clientId_,
					a_diagnostic.source, a_diagnostic.message);
			else
				REX::ERROR("{} mod=\"{}\" client_id=\"{}\" {}: {}"sv,
					kDiagnosticLogTag, mod_, clientId_,
					a_diagnostic.source, a_diagnostic.message);
		}
		else
		{
			if (a_diagnostic.severity == DiagnosticSeverity::kWarning)
				REX::WARN("{} mod=\"{}\" client_id=\"{}\" {}: {}: {}"sv,
					kDiagnosticLogTag, mod_, clientId_,
					a_diagnostic.source, a_diagnostic.location,
					a_diagnostic.message);
			else
				REX::ERROR("{} mod=\"{}\" client_id=\"{}\" {}: {}: {}"sv,
					kDiagnosticLogTag, mod_, clientId_,
					a_diagnostic.source, a_diagnostic.location,
					a_diagnostic.message);
		}
		Submit(a_diagnostic);
	}

	void RexDiagnosticReporter::AttachClient(dmui::Client& a_client) noexcept
	{
		const std::scoped_lock lock{ mutex_ };
		client_ = &a_client;
		for (const auto& diagnostic : pending_)
			Send(a_client, diagnostic);
		pending_.clear();
	}

	void RexDiagnosticReporter::ReportSummary(
		DMUI_StatusSeverity a_severity,
		std::string_view a_scope,
		std::string_view a_summary,
		std::string_view a_detail) noexcept
	{
		Submit(a_severity, a_scope, a_summary, a_detail);
	}

	void RexDiagnosticReporter::Submit(
		const Diagnostic& a_diagnostic) noexcept
	{
		Submit(
			ToStatusSeverity(a_diagnostic.severity),
			a_diagnostic.source,
			a_diagnostic.message,
			a_diagnostic.location);
	}

	void RexDiagnosticReporter::Submit(
		DMUI_StatusSeverity a_severity,
		std::string_view a_scope,
		std::string_view a_summary,
		std::string_view a_detail) noexcept
	{
		try
		{
			const std::scoped_lock lock{ mutex_ };
			if (client_)
			{
				const PendingDiagnostic diagnostic{
					a_severity,
					std::string{ a_scope },
					std::string{ a_summary },
					std::string{ a_detail }
				};
				Send(*client_, diagnostic);
				return;
			}
			pending_.push_back({
				a_severity,
				std::string{ a_scope },
				std::string{ a_summary },
				std::string{ a_detail }
			});
		}
		catch (const std::exception& a_error)
		{
			REX::ERROR(
				"{} mod=\"{}\" client_id=\"{}\" host report failed: {}"sv,
				kDiagnosticLogTag,
				mod_,
				clientId_,
				a_error.what());
		}
		catch (...)
		{
			REX::ERROR(
				"{} mod=\"{}\" client_id=\"{}\" host report failed"sv,
				kDiagnosticLogTag,
				mod_,
				clientId_);
		}
	}

	void RexDiagnosticReporter::Send(
		dmui::Client& a_client,
		const PendingDiagnostic& a_diagnostic) noexcept
	{
		if (a_client.ReportDiagnostic({
				a_diagnostic.severity,
				a_diagnostic.scope.empty() ?
					nullptr :
					a_diagnostic.scope.c_str(),
				a_diagnostic.summary.c_str(),
				a_diagnostic.detail.empty() ?
					nullptr :
					a_diagnostic.detail.c_str()
			}))
			return;
		REX::ERROR(
			"{} mod=\"{}\" client_id=\"{}\" host rejected diagnostic ({})"sv,
			kDiagnosticLogTag,
			mod_,
			clientId_,
			DMUI_ResultToString(a_client.LastResult()));
	}
}
