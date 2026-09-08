#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace DearModdingUI::MCM::ScaleformSpike
{
	enum class Fact : std::uint8_t
	{
		kNotObserved,
		kMissing,
		kPresent
	};

	enum class CallFact : std::uint8_t
	{
		kNotAttempted,
		kMethodMissing,
		kFailed,
		kSucceeded
	};

	struct MovieObservation
	{
		std::string context{ "not observed" };
		std::string sourceMovieUrl{ "not observed" };
		std::string loaderInfoUrl{ "not observed" };
		Fact root{ Fact::kNotObserved };
		Fact f4se{ Fact::kNotObserved };
		Fact mcm{ Fact::kNotObserved };
		Fact menu{ Fact::kNotObserved };
		std::string pauseModePath{ "not observed" };
		std::string pauseModeMember{ "not observed" };
		Fact bgsCodeObject{ Fact::kNotObserved };
		Fact setBackgroundVisible{ Fact::kNotObserved };
		Fact mcmLoaderContent{ Fact::kNotObserved };
		Fact mcmDocumentMenu{ Fact::kNotObserved };
		Fact mcmCodeObject{ Fact::kNotObserved };
		Fact mcmVersionMethod{ Fact::kNotObserved };
		CallFact mcmVersionCall{ CallFact::kNotAttempted };
		std::string mcmVersionResult{ "not attempted" };
		bool movieVisible{};
		bool timingInitialized{};
		std::uint64_t initialTimerMs{};
		std::uint64_t currentTimerMs{};
		std::uint32_t initialFrame{};
		std::uint32_t currentFrame{};
		std::uint64_t advanceCalls{};
		double elapsedSeconds{};
	};

	[[nodiscard]] bool Advanced(const MovieObservation& a_observation) noexcept;
	[[nodiscard]] bool ContextGatePassed(const MovieObservation& a_observation) noexcept;
	[[nodiscard]] std::string MissingContextReason(const MovieObservation& a_observation);

	enum class Phase : std::uint8_t
	{
		kIdle,
		kStartQueued,
		kLoading,
		kRunning,
		kReleasing,
		kPassed,
		kBlocked,
		kTimedOut,
		kStopped,
		kCleanupBlocked
	};

	enum class Terminal : std::uint8_t
	{
		kPassed,
		kBlocked,
		kTimedOut,
		kStopped
	};

	struct StartRequest
	{
		bool accepted{};
		std::uint64_t generation{};
	};

	class RunState
	{
	public:
		[[nodiscard]] StartRequest RequestStart() noexcept;
		[[nodiscard]] bool BeginLoad(std::uint64_t a_generation) noexcept;
		[[nodiscard]] bool AcquireResource(std::uint64_t a_generation) noexcept;
		[[nodiscard]] bool MarkRunning(std::uint64_t a_generation) noexcept;
		[[nodiscard]] bool BeginTerminal(
			std::uint64_t a_generation,
			Terminal a_terminal) noexcept;
		[[nodiscard]] std::optional<std::uint64_t> Cancel(
			Terminal a_terminal = Terminal::kStopped) noexcept;
		[[nodiscard]] bool MarkCleanupBlocked(
			std::uint64_t a_generation) noexcept;
		[[nodiscard]] bool CompleteRelease(
			std::uint64_t a_generation) noexcept;

		[[nodiscard]] bool IsCurrent(std::uint64_t a_generation) const noexcept;
		[[nodiscard]] bool IsBusy() const noexcept;
		[[nodiscard]] bool NeedsOwnerTick() const noexcept;
		[[nodiscard]] bool ResourceRetained() const noexcept;
		[[nodiscard]] std::uint64_t Generation() const noexcept;
		[[nodiscard]] Phase CurrentPhase() const noexcept;
		[[nodiscard]] Terminal PendingTerminal() const noexcept;

	private:
		[[nodiscard]] static Phase TerminalPhase(Terminal a_terminal) noexcept;

		std::uint64_t generation_{};
		Phase phase_{ Phase::kIdle };
		Terminal pendingTerminal_{ Terminal::kStopped };
		bool resourceRetained_{};
	};
}
