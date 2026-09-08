#pragma once

#include <algorithm>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DearModdingUI
{
	using HealthClock = std::chrono::steady_clock;

	class SubsystemHealth;

	enum class HealthState
	{
		kWaiting,
		kProgressing,
		kReady,
		kDegraded,
		kFailed
	};

	enum class HealthSeverity
	{
		kNeutral,
		kInfo,
		kSuccess,
		kWarning,
		kError
	};

	enum class HealthEvent
	{
		kTransition,
		kDeadlineExceeded,
		kDeadlineProgress,
		kDeadlineRecovery,
		kRecovery
	};

	[[nodiscard]] constexpr std::string_view HealthStateLabel(
		HealthState a_state) noexcept
	{
		switch (a_state)
		{
		case HealthState::kWaiting:
			return "Waiting";
		case HealthState::kProgressing:
			return "Progressing";
		case HealthState::kReady:
			return "Ready";
		case HealthState::kDegraded:
			return "Degraded";
		case HealthState::kFailed:
			return "Failed";
		default:
			return "Unknown";
		}
	}

	[[nodiscard]] constexpr HealthSeverity HealthStateSeverity(
		HealthState a_state) noexcept
	{
		switch (a_state)
		{
		case HealthState::kWaiting:
			return HealthSeverity::kNeutral;
		case HealthState::kProgressing:
			return HealthSeverity::kInfo;
		case HealthState::kReady:
			return HealthSeverity::kSuccess;
		case HealthState::kDegraded:
			return HealthSeverity::kWarning;
		case HealthState::kFailed:
			return HealthSeverity::kError;
		default:
			return HealthSeverity::kInfo;
		}
	}

	[[nodiscard]] constexpr bool HealthStateIsReady(
		HealthState a_state) noexcept
	{
		return a_state == HealthState::kReady;
	}

	[[nodiscard]] constexpr bool HealthStateIsUsable(
		HealthState a_state) noexcept
	{
		return a_state == HealthState::kReady ||
			a_state == HealthState::kDegraded;
	}

	[[nodiscard]] constexpr bool HealthStateIsStarting(
		HealthState a_state) noexcept
	{
		return a_state == HealthState::kWaiting ||
			a_state == HealthState::kProgressing;
	}

	[[nodiscard]] constexpr bool HealthStateNeedsAttention(
		HealthState a_state) noexcept
	{
		return a_state == HealthState::kDegraded ||
			a_state == HealthState::kFailed;
	}

	struct HealthObservation
	{
		HealthState state{ HealthState::kWaiting };
		std::string reason;
	};

	struct HealthSnapshot
	{
		std::string identity;
		HealthState state{ HealthState::kWaiting };
		HealthClock::time_point enteredAt{};
		std::optional<HealthClock::time_point> deadline;
		std::string reason;
	};

	[[nodiscard]] inline bool HealthDeadlineExceeded(
		const HealthSnapshot& a_snapshot,
		HealthClock::time_point a_now) noexcept
	{
		return !HealthStateIsReady(a_snapshot.state) &&
			a_snapshot.deadline && a_now >= *a_snapshot.deadline;
	}

	[[nodiscard]] inline bool HealthNeedsAttention(
		const HealthSnapshot& a_snapshot,
		HealthClock::time_point a_now) noexcept
	{
		return HealthStateNeedsAttention(a_snapshot.state) ||
			HealthDeadlineExceeded(a_snapshot, a_now);
	}

	[[nodiscard]] inline HealthSeverity HealthSnapshotSeverity(
		const HealthSnapshot& a_snapshot,
		HealthClock::time_point a_now) noexcept
	{
		const auto severity = HealthStateSeverity(a_snapshot.state);
		if (severity == HealthSeverity::kError)
			return severity;
		return HealthDeadlineExceeded(a_snapshot, a_now) ?
			HealthSeverity::kWarning :
			severity;
	}

	class HealthReporter
	{
	public:
		HealthReporter() = default;
		virtual ~HealthReporter() = default;

		HealthReporter(const HealthReporter&) = delete;
		HealthReporter(HealthReporter&&) = delete;
		HealthReporter& operator=(const HealthReporter&) = delete;
		HealthReporter& operator=(HealthReporter&&) = delete;

		virtual void Report(
			HealthEvent a_event,
			const HealthSnapshot& a_snapshot) noexcept = 0;
	};

	class SubsystemHealthRegistry
	{
	public:
		SubsystemHealthRegistry() = default;

		SubsystemHealthRegistry(const SubsystemHealthRegistry&) = delete;
		SubsystemHealthRegistry(SubsystemHealthRegistry&&) = delete;
		SubsystemHealthRegistry& operator=(const SubsystemHealthRegistry&) = delete;
		SubsystemHealthRegistry& operator=(SubsystemHealthRegistry&&) = delete;

		[[nodiscard]] std::vector<HealthSnapshot> Snapshots() const;

	private:
		friend class SubsystemHealth;

		void Register(const SubsystemHealth& a_health);
		void Unregister(const SubsystemHealth& a_health);

		mutable std::mutex mutex_;
		std::vector<const SubsystemHealth*> subsystems_;
	};

	[[nodiscard]] inline SubsystemHealthRegistry& HostSubsystemHealthRegistry()
	{
		static SubsystemHealthRegistry registry;
		return registry;
	}

	class SubsystemHealth
	{
	public:
		SubsystemHealth(
			std::string_view a_identity,
			HealthReporter& a_reporter,
			HealthClock::time_point a_now = HealthClock::now()) :
			reporter_(a_reporter)
		{
			identity_ = a_identity;
			snapshot_.identity = identity_;
			snapshot_.enteredAt = a_now;
		}

		SubsystemHealth(
			std::string_view a_identity,
			HealthReporter& a_reporter,
			SubsystemHealthRegistry& a_registry,
			HealthClock::time_point a_now = HealthClock::now()) :
			SubsystemHealth(a_identity, a_reporter, a_now)
		{
			registry_ = &a_registry;
			registry_->Register(*this);
		}

		~SubsystemHealth()
		{
			if (registry_)
				registry_->Unregister(*this);
		}

		bool Observe(
			HealthState a_state,
			std::string_view a_reason,
			HealthClock::time_point a_now = HealthClock::now()) noexcept
		{
			HealthSnapshot candidate;
			HealthSnapshot reportedSnapshot;
			try
			{
				candidate.identity = identity_;
				candidate.state = a_state;
				candidate.enteredAt = a_now;
				candidate.reason = a_reason;
				reportedSnapshot = candidate;
			}
			catch (...)
			{
				return false;
			}

			HealthEvent event{ HealthEvent::kTransition };
			{
				const std::scoped_lock lock{ mutex_ };
				if (reported_ &&
					snapshot_.state == a_state &&
					snapshot_.reason == a_reason)
					return true;

				candidate.deadline = snapshot_.deadline;
				reportedSnapshot.deadline = snapshot_.deadline;
				if (a_state == HealthState::kDegraded ||
					a_state == HealthState::kFailed)
					fullRecoveryPending_ = true;
				if (deadlineRecoveryPending_)
				{
					if (a_state == HealthState::kReady)
					{
						event = HealthEvent::kDeadlineRecovery;
						deadlineRecoveryPending_ = false;
						fullRecoveryPending_ = false;
					}
					else if (a_state == HealthState::kProgressing)
					{
						event = HealthEvent::kDeadlineProgress;
					}
				}
				else if (reported_ &&
					fullRecoveryPending_ &&
					a_state == HealthState::kReady)
				{
					event = HealthEvent::kRecovery;
					fullRecoveryPending_ = false;
				}

				snapshot_ = std::move(candidate);
				reported_ = true;
			}
			reporter_.Report(event, reportedSnapshot);
			return true;
		}

		void SetDeadline(
			std::optional<HealthClock::time_point> a_deadline) noexcept
		{
			const std::scoped_lock lock{ mutex_ };
			snapshot_.deadline = a_deadline;
			deadlineReported_ = false;
			deadlineRecoveryPending_ = false;
		}

		bool Evaluate(
			HealthClock::time_point a_now = HealthClock::now()) noexcept
		{
			HealthSnapshot snapshot;
			{
				const std::scoped_lock lock{ mutex_ };
				if (snapshot_.state == HealthState::kReady ||
					!snapshot_.deadline ||
					a_now < *snapshot_.deadline ||
					deadlineReported_)
					return true;

				try
				{
					snapshot = snapshot_;
				}
				catch (...)
				{
					return false;
				}
				deadlineReported_ = true;
				deadlineRecoveryPending_ = true;
			}
			reporter_.Report(HealthEvent::kDeadlineExceeded, snapshot);
			return true;
		}

		void InvalidateObservation() noexcept
		{
			const std::scoped_lock lock{ mutex_ };
			reported_ = false;
		}

		[[nodiscard]] HealthSnapshot Snapshot() const
		{
			const std::scoped_lock lock{ mutex_ };
			return snapshot_;
		}

	private:
		[[nodiscard]] std::optional<HealthSnapshot> ObservedSnapshot() const
		{
			const std::scoped_lock lock{ mutex_ };
			if (!reported_)
				return std::nullopt;
			return snapshot_;
		}

		friend class SubsystemHealthRegistry;

		HealthReporter& reporter_;
		std::string identity_;
		SubsystemHealthRegistry* registry_{};
		mutable std::mutex mutex_;
		HealthSnapshot snapshot_;
		bool reported_{ false };
		bool deadlineReported_{ false };
		bool deadlineRecoveryPending_{ false };
		bool fullRecoveryPending_{ false };
	};

	inline void SubsystemHealthRegistry::Register(
		const SubsystemHealth& a_health)
	{
		const std::scoped_lock lock{ mutex_ };
		subsystems_.push_back(&a_health);
	}

	inline void SubsystemHealthRegistry::Unregister(
		const SubsystemHealth& a_health)
	{
		const std::scoped_lock lock{ mutex_ };
		const auto found = std::ranges::find(subsystems_, &a_health);
		if (found != subsystems_.end())
			subsystems_.erase(found);
	}

	inline std::vector<HealthSnapshot> SubsystemHealthRegistry::Snapshots() const
	{
		const std::scoped_lock lock{ mutex_ };
		std::vector<HealthSnapshot> snapshots;
		snapshots.reserve(subsystems_.size());
		for (const auto* subsystem : subsystems_)
		{
			if (const auto snapshot = subsystem->ObservedSnapshot())
				snapshots.push_back(*snapshot);
		}
		return snapshots;
	}
}
