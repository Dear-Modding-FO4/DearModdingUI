#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/Status.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace DearModdingUI
{
	inline constexpr size_t kDiagnosticRecordLimitPerClient{ 16 };

	struct ClientDiagnosticRecord
	{
		DMUI_ClientHandle client{ DMUI_INVALID_CLIENT_HANDLE };
		DMUI_StatusSeverity severity{ DMUI_STATUS_SEVERITY_INFO };
		std::string scope;
		std::string summary;
		std::string detail;
		uint64_t occurrenceCount{ 1 };
	};

	struct ClientDiagnosticSnapshot
	{
		DMUI_ClientHandle client{ DMUI_INVALID_CLIENT_HANDLE };
		std::vector<ClientDiagnosticRecord> records;
		size_t droppedDistinctCount{};
	};

	[[nodiscard]] inline DMUI_Result DMUI_CALL ValidateDiagnosticArguments(
		DMUI_ClientHandle,
		const DMUI_DiagnosticDescriptor* a_diagnostic) noexcept
	{
		if (!a_diagnostic ||
			a_diagnostic->structSize < DMUI_DIAGNOSTIC_DESCRIPTOR_0_1_SIZE ||
			!IsValidStatusSeverity(a_diagnostic->severity) ||
			!a_diagnostic->summary || !a_diagnostic->summary[0])
			return DMUI_RESULT_INVALID_ARGUMENT;
		return DMUI_RESULT_OK;
	}

	class DiagnosticStore
	{
	public:
		[[nodiscard]] DMUI_Result Report(
			DMUI_ClientHandle a_client,
			const DMUI_DiagnosticDescriptor& a_diagnostic) noexcept;
		[[nodiscard]] std::optional<ClientDiagnosticSnapshot> Snapshot(
			DMUI_ClientHandle a_client) const noexcept;
		[[nodiscard]] std::vector<ClientDiagnosticSnapshot> Snapshots() const noexcept;

	private:
		mutable std::mutex mutex_;
		std::vector<ClientDiagnosticSnapshot> clients_;
	};
}
