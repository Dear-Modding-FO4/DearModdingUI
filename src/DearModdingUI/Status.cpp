#include <DearModdingUI/Status.h>

#include <algorithm>
#include <limits>

namespace DearModdingUI
{
	std::vector<ClientStatus> RollupClientStatuses(
		std::span<const ClientStatus> a_statuses)
	{
		std::vector<ClientStatus> rollups;
		for (const auto& status : a_statuses)
		{
			if (status.client == DMUI_INVALID_CLIENT_HANDLE ||
				!IsValidStatusSeverity(status.severity))
				continue;
			const auto found = std::ranges::find(
				rollups,
				status.client,
				&ClientStatus::client);
			if (found == rollups.end())
				rollups.push_back(status);
			else if (status.severity > found->severity)
				found->severity = status.severity;
		}
		std::ranges::sort(rollups, {}, &ClientStatus::client);
		return rollups;
	}

	DMUI_Result StatusModel::Set(
		StatusOwnerKind a_ownerKind,
		std::string_view a_owner,
		DMUI_StatusSeverity a_severity,
		std::string_view a_message,
		StatusClock::time_point a_now) noexcept
	{
		return SetImpl(
			a_ownerKind,
			DMUI_INVALID_CLIENT_HANDLE,
			a_owner,
			a_severity,
			a_message,
			a_now);
	}

	DMUI_Result StatusModel::SetClient(
		DMUI_ClientHandle a_client,
		std::string_view a_owner,
		DMUI_StatusSeverity a_severity,
		std::string_view a_message,
		StatusClock::time_point a_now) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		return SetImpl(
			StatusOwnerKind::kClient,
			a_client,
			a_owner,
			a_severity,
			a_message,
			a_now);
	}

	DMUI_Result StatusModel::SetImpl(
		StatusOwnerKind a_ownerKind,
		DMUI_ClientHandle a_client,
		std::string_view a_owner,
		DMUI_StatusSeverity a_severity,
		std::string_view a_message,
		StatusClock::time_point a_now) noexcept
	{
		if (a_owner.empty() ||
			a_message.empty() ||
			!IsValidStatusSeverity(a_severity))
			return DMUI_RESULT_INVALID_ARGUMENT;

		try
		{
			StatusMessage next;
			next.ownerKind = a_ownerKind;
			next.severity = a_severity;
			next.owner.assign(a_owner);
			next.message.assign(a_message);
			next.attributedText.reserve(
				next.owner.size() + next.message.size() + 2);
			next.attributedText.append(next.owner);
			next.attributedText.append(": ");
			next.attributedText.append(next.message);
			next.createdAt = a_now;
			next.persistent = IsPersistentStatus(a_severity);

			const std::scoped_lock lock{ m_mutex };
			const auto nextGeneration =
				m_generation == (std::numeric_limits<uint64_t>::max)() ?
				uint64_t{ 1 } :
				m_generation + 1;
			next.generation = nextGeneration;

			auto clientStatuses = m_clientStatuses;
			if (a_client != DMUI_INVALID_CLIENT_HANDLE)
			{
				const auto found = std::ranges::find(
					clientStatuses,
					a_client,
					&std::pair<DMUI_ClientHandle, StatusMessage>::first);
				if (found == clientStatuses.end())
					clientStatuses.emplace_back(a_client, next);
				else
					found->second = next;
			}

			m_generation = nextGeneration;
			m_clientStatuses = std::move(clientStatuses);
			m_current = std::move(next);
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	std::optional<StatusMessage> StatusModel::Snapshot(
		StatusClock::time_point a_now) noexcept
	{
		try
		{
			const std::scoped_lock lock{ m_mutex };
			if (m_current &&
				!m_current->persistent &&
				a_now >= m_current->createdAt + kTransientStatusLifetime)
				m_current.reset();
			return m_current;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	std::vector<ClientStatus> StatusModel::SnapshotClientStatuses(
		StatusClock::time_point a_now) noexcept
	{
		try
		{
			const std::scoped_lock lock{ m_mutex };
			std::erase_if(m_clientStatuses, [&](const auto& a_entry) {
				return !a_entry.second.persistent &&
					a_now >= a_entry.second.createdAt + kTransientStatusLifetime;
			});
			std::vector<ClientStatus> statuses;
			statuses.reserve(m_clientStatuses.size());
			for (const auto& [client, message] : m_clientStatuses)
				statuses.push_back({ client, message.severity });
			return RollupClientStatuses(statuses);
		}
		catch (...)
		{
			return {};
		}
	}

	bool StatusModel::Dismiss(uint64_t a_generation) noexcept
	{
		const std::scoped_lock lock{ m_mutex };
		if (!m_current ||
			!m_current->persistent ||
			m_current->generation != a_generation)
			return false;
		std::erase_if(m_clientStatuses, [&](const auto& a_entry) {
			return a_entry.second.generation == a_generation;
		});
		m_current.reset();
		return true;
	}
}
