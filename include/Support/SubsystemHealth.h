#pragma once

#include <chrono>
#include <optional>
#include <string_view>

namespace DearModdingUI
{
	using HealthClock = std::chrono::steady_clock;

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

		void Observe(
			HealthState a_state,
			std::string_view a_reason,
			HealthClock::time_point a_now = HealthClock::now()) noexcept
		{
			if (reported_ &&
				snapshot_.state == a_state &&
				snapshot_.reason == a_reason)
				return;

			snapshot_.state = a_state;
			snapshot_.reason = a_reason;
			snapshot_.enteredAt = a_now;
			reported_ = true;
			const auto recovered =
				recoveryPending_ && a_state != HealthState::kWaiting;
			if (recovered)
				recoveryPending_ = false;
			reporter_.Report(
				recovered ? HealthEvent::kRecovery : HealthEvent::kTransition,
				snapshot_);
		}

		void SetDeadline(
			std::optional<HealthClock::time_point> a_deadline) noexcept
		{
			snapshot_.deadline = a_deadline;
			deadlineReported_ = false;
			recoveryPending_ = false;
		}

		void Evaluate(
			HealthClock::time_point a_now = HealthClock::now()) noexcept
		{
			if (snapshot_.state == HealthState::kReady ||
				!snapshot_.deadline ||
				a_now < *snapshot_.deadline ||
				deadlineReported_)
				return;

			deadlineReported_ = true;
			recoveryPending_ = true;
			reporter_.Report(HealthEvent::kDeadlineExceeded, snapshot_);
		}

		void InvalidateObservation() noexcept
		{
			reported_ = false;
		}

		[[nodiscard]] const HealthSnapshot& Snapshot() const noexcept
		{
			return snapshot_;
		}

	private:
		HealthReporter& reporter_;
		HealthSnapshot snapshot_;
		bool reported_{ false };
		bool deadlineReported_{ false };
		bool recoveryPending_{ false };
	};
}
