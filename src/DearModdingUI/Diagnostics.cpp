#include <DearModdingUI/Diagnostics.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <new>
#include <string_view>

namespace DearModdingUI
{
	DMUI_Result DiagnosticStore::Report(
		DMUI_ClientHandle a_client,
		const DMUI_DiagnosticDescriptor& a_diagnostic) noexcept
	{
		try
		{
			const std::string_view scope{
				a_diagnostic.scope ? a_diagnostic.scope : ""
			};
			const std::string_view summary{ a_diagnostic.summary };
			const std::string_view detail{
				a_diagnostic.detail ? a_diagnostic.detail : ""
			};
			const std::scoped_lock lock{ mutex_ };
			auto client = std::ranges::find(
				clients_,
				a_client,
				&ClientDiagnosticSnapshot::client);
			if (client == clients_.end())
			{
				clients_.push_back({ .client = a_client });
				client = std::prev(clients_.end());
			}

			const auto record = std::ranges::find_if(
				client->records,
				[&](const ClientDiagnosticRecord& a_record) {
					return a_record.severity == a_diagnostic.severity &&
						a_record.scope == scope &&
						a_record.summary == summary;
				});
			if (record != client->records.end())
			{
				if (record->occurrenceCount <
					(std::numeric_limits<uint64_t>::max)())
					++record->occurrenceCount;
				return DMUI_RESULT_OK;
			}
			if (client->records.size() >= kDiagnosticRecordLimitPerClient)
			{
				if (client->droppedReportCount <
					(std::numeric_limits<size_t>::max)())
					++client->droppedReportCount;
				return DMUI_RESULT_OK;
			}

			client->records.push_back({
				a_client,
				a_diagnostic.severity,
				std::string{ scope },
				std::string{ summary },
				std::string{ detail },
				1
			});
			return DMUI_RESULT_OK;
		}
		catch (const std::bad_alloc&)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
		catch (...)
		{
			return DMUI_RESULT_CALLBACK_FAILED;
		}
	}

	std::optional<ClientDiagnosticSnapshot> DiagnosticStore::Snapshot(
		DMUI_ClientHandle a_client) const noexcept
	{
		try
		{
			const std::scoped_lock lock{ mutex_ };
			const auto client = std::ranges::find(
				clients_,
				a_client,
				&ClientDiagnosticSnapshot::client);
			if (client == clients_.end())
				return std::nullopt;
			return *client;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	std::vector<ClientDiagnosticSnapshot> DiagnosticStore::Snapshots() const noexcept
	{
		try
		{
			const std::scoped_lock lock{ mutex_ };
			return clients_;
		}
		catch (...)
		{
			return {};
		}
	}
}
