#pragma once

#include <algorithm>
#include <chrono>
#include <mutex>
#include <optional>
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
		kReady
	};

	enum class HealthEvent
	{
		kTransition,
		kDeadlineExceeded,
		kRecovery
	};

	struct HealthSnapshot
	{
		std::string_view identity;
		HealthState state{ HealthState::kWaiting };
		HealthClock::time_point enteredAt{};
		std::optional<HealthClock::time_point> deadline;
		std::string_view reason;
	};

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
			HealthClock::time_point a_now = HealthClock::now()) noexcept :
			reporter_(a_reporter)
		{
			snapshot_.identity = a_identity;
			snapshot_.enteredAt = a_now;
		}

		SubsystemHealth(
			std::string_view a_identity,
			HealthReporter& a_reporter,
			SubsystemHealthRegistry& a_registry,
			HealthClock::time_point a_now = HealthClock::now()) noexcept :
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

		void Observe(
			HealthState a_state,
			std::string_view a_reason,
			HealthClock::time_point a_now = HealthClock::now()) noexcept
		{
			HealthSnapshot snapshot;
			bool recovered{};
			{
				const std::scoped_lock lock{ mutex_ };
				if (reported_ &&
					snapshot_.state == a_state &&
					snapshot_.reason == a_reason)
					return;

				snapshot_.state = a_state;
				snapshot_.reason = a_reason;
				snapshot_.enteredAt = a_now;
				reported_ = true;
				recovered =
					recoveryPending_ && a_state != HealthState::kWaiting;
				if (recovered)
					recoveryPending_ = false;
				snapshot = snapshot_;
			}
			reporter_.Report(
				recovered ? HealthEvent::kRecovery : HealthEvent::kTransition,
				snapshot);
		}

		void SetDeadline(
			std::optional<HealthClock::time_point> a_deadline) noexcept
		{
			const std::scoped_lock lock{ mutex_ };
			snapshot_.deadline = a_deadline;
			deadlineReported_ = false;
			recoveryPending_ = false;
		}

		void Evaluate(
			HealthClock::time_point a_now = HealthClock::now()) noexcept
		{
			HealthSnapshot snapshot;
			{
				const std::scoped_lock lock{ mutex_ };
				if (snapshot_.state == HealthState::kReady ||
					!snapshot_.deadline ||
					a_now < *snapshot_.deadline ||
					deadlineReported_)
					return;

				deadlineReported_ = true;
				recoveryPending_ = true;
				snapshot = snapshot_;
			}
			reporter_.Report(HealthEvent::kDeadlineExceeded, snapshot);
		}

		void InvalidateObservation() noexcept
		{
			const std::scoped_lock lock{ mutex_ };
			reported_ = false;
		}

		[[nodiscard]] HealthSnapshot Snapshot() const noexcept
		{
			const std::scoped_lock lock{ mutex_ };
			return snapshot_;
		}

	private:
		[[nodiscard]] std::optional<HealthSnapshot> ObservedSnapshot() const noexcept
		{
			const std::scoped_lock lock{ mutex_ };
			if (!reported_)
				return std::nullopt;
			return snapshot_;
		}

		friend class SubsystemHealthRegistry;

		HealthReporter& reporter_;
		SubsystemHealthRegistry* registry_{};
		mutable std::mutex mutex_;
		HealthSnapshot snapshot_;
		bool reported_{ false };
		bool deadlineReported_{ false };
		bool recoveryPending_{ false };
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
