#include <DearModdingUI/MCM/ScaleformSpikeState.h>

#include <algorithm>
#include <array>
#include <string_view>

namespace DearModdingUI::MCM::ScaleformSpike
{
	namespace
	{
		struct ContextCheck
		{
			std::string_view label;
			bool available;
		};

		[[nodiscard]] auto ContextChecks(const MovieObservation& a_observation) noexcept
		{
			return std::array{
				ContextCheck{ "root", a_observation.root == Fact::kPresent },
				ContextCheck{ "root.f4se", a_observation.f4se == Fact::kPresent },
				ContextCheck{ "root.mcm", a_observation.mcm == Fact::kPresent },
				ContextCheck{ "root.Menu_mc", a_observation.menu == Fact::kPresent },
				ContextCheck{ "genuine Menu_mc.BGSCodeObj", a_observation.bgsCodeObject == Fact::kPresent },
				ContextCheck{ "BGSCodeObj.SetBackgroundVisible", a_observation.setBackgroundVisible == Fact::kPresent },
				ContextCheck{ "mcm_loader.content", a_observation.mcmLoaderContent == Fact::kPresent },
				ContextCheck{ "MCM_Main.mcmMenu", a_observation.mcmDocumentMenu == Fact::kPresent },
				ContextCheck{ "mcmMenu.mcmCodeObj", a_observation.mcmCodeObject == Fact::kPresent },
				ContextCheck{ "GetMCMVersionCode", a_observation.mcmVersionMethod == Fact::kPresent },
				ContextCheck{ "native callback dispatch", a_observation.mcmVersionCall == CallFact::kSucceeded },
				ContextCheck{ "movie timer/frame advancement", Advanced(a_observation) }
			};
		}
	}

	bool Advanced(const MovieObservation& a_observation) noexcept
	{
		return a_observation.timingInitialized &&
			(a_observation.currentTimerMs > a_observation.initialTimerMs ||
				a_observation.currentFrame != a_observation.initialFrame);
	}

	bool ContextGatePassed(const MovieObservation& a_observation) noexcept
	{
		return std::ranges::all_of(
			ContextChecks(a_observation),
			[](const ContextCheck& a_check) { return a_check.available; });
	}

	std::string MissingContextReason(const MovieObservation& a_observation)
	{
		std::string reason{ "Blocked at the 5 second context deadline; missing:" };
		for (const auto& check : ContextChecks(a_observation))
			if (!check.available)
				reason.append(" ").append(check.label).append(";");
		return reason;
	}

	StartRequest RunState::RequestStart() noexcept
	{
		if (IsBusy() || resourceRetained_)
			return { false, generation_ };

		++generation_;
		phase_ = Phase::kStartQueued;
		pendingTerminal_ = Terminal::kStopped;
		return { true, generation_ };
	}

	bool RunState::BeginLoad(std::uint64_t a_generation) noexcept
	{
		if (!IsCurrent(a_generation) || phase_ != Phase::kStartQueued)
			return false;
		phase_ = Phase::kLoading;
		return true;
	}

	bool RunState::AcquireResource(std::uint64_t a_generation) noexcept
	{
		if (!IsCurrent(a_generation) || phase_ != Phase::kLoading ||
			resourceRetained_)
			return false;
		resourceRetained_ = true;
		return true;
	}

	bool RunState::MarkRunning(std::uint64_t a_generation) noexcept
	{
		if (!IsCurrent(a_generation) || phase_ != Phase::kLoading ||
			!resourceRetained_)
			return false;
		phase_ = Phase::kRunning;
		return true;
	}

	bool RunState::BeginTerminal(
		std::uint64_t a_generation,
		Terminal a_terminal) noexcept
	{
		if (!IsCurrent(a_generation) ||
			(phase_ != Phase::kStartQueued &&
				phase_ != Phase::kLoading &&
				phase_ != Phase::kRunning))
			return false;
		pendingTerminal_ = a_terminal;
		phase_ = resourceRetained_ ?
			Phase::kReleasing :
			TerminalPhase(a_terminal);
		return true;
	}

	std::optional<std::uint64_t> RunState::Cancel(
		Terminal a_terminal) noexcept
	{
		if (!IsBusy() && !resourceRetained_)
			return std::nullopt;
		++generation_;
		if (phase_ != Phase::kReleasing && phase_ != Phase::kCleanupBlocked)
		{
			pendingTerminal_ = a_terminal;
			phase_ = resourceRetained_ ?
				Phase::kReleasing :
				TerminalPhase(a_terminal);
		}
		return generation_;
	}

	bool RunState::MarkCleanupBlocked(
		std::uint64_t a_generation) noexcept
	{
		if (!IsCurrent(a_generation) || phase_ != Phase::kReleasing ||
			!resourceRetained_)
			return false;
		phase_ = Phase::kCleanupBlocked;
		return true;
	}

	bool RunState::CompleteRelease(std::uint64_t a_generation) noexcept
	{
		if (!IsCurrent(a_generation) ||
			(phase_ != Phase::kReleasing &&
				phase_ != Phase::kCleanupBlocked) ||
			!resourceRetained_)
			return false;
		resourceRetained_ = false;
		phase_ = TerminalPhase(pendingTerminal_);
		return true;
	}

	bool RunState::IsCurrent(std::uint64_t a_generation) const noexcept
	{
		return generation_ == a_generation;
	}

	bool RunState::IsBusy() const noexcept
	{
		switch (phase_)
		{
		case Phase::kStartQueued:
		case Phase::kLoading:
		case Phase::kRunning:
		case Phase::kReleasing:
		case Phase::kCleanupBlocked:
			return true;
		default:
			return false;
		}
	}

	bool RunState::NeedsOwnerTick() const noexcept
	{
		return phase_ == Phase::kRunning ||
			phase_ == Phase::kReleasing ||
			phase_ == Phase::kCleanupBlocked;
	}

	bool RunState::ResourceRetained() const noexcept
	{
		return resourceRetained_;
	}

	std::uint64_t RunState::Generation() const noexcept
	{
		return generation_;
	}

	Phase RunState::CurrentPhase() const noexcept
	{
		return phase_;
	}

	Terminal RunState::PendingTerminal() const noexcept
	{
		return pendingTerminal_;
	}

	Phase RunState::TerminalPhase(Terminal a_terminal) noexcept
	{
		switch (a_terminal)
		{
		case Terminal::kPassed:
			return Phase::kPassed;
		case Terminal::kBlocked:
			return Phase::kBlocked;
		case Terminal::kTimedOut:
			return Phase::kTimedOut;
		case Terminal::kStopped:
		default:
			return Phase::kStopped;
		}
	}
}
