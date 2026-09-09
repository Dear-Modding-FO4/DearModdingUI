#include <DearModdingUI/MCM/ScaleformSpikeState.h>

#include "Harness.h"

#include <array>
#include <string_view>

namespace vmm_tests
{
	namespace
	{
		[[nodiscard]] DearModdingUI::MCM::ScaleformSpike::MovieObservation CompleteContext()
		{
			using namespace DearModdingUI::MCM::ScaleformSpike;
			return {
				.root = Fact::kPresent,
				.f4se = Fact::kPresent,
				.mcm = Fact::kPresent,
				.menu = Fact::kPresent,
				.bgsCodeObject = Fact::kPresent,
				.setBackgroundVisible = Fact::kPresent,
				.mcmLoaderContent = Fact::kPresent,
				.mcmDocumentMenu = Fact::kPresent,
				.mcmCodeObject = Fact::kPresent,
				.mcmVersionMethod = Fact::kPresent,
				.mcmVersionCall = CallFact::kSucceeded,
				.mcmVersionResult = "11",
				.timingInitialized = true,
				.initialTimerMs = 16,
				.currentTimerMs = 5032,
				.advanceCalls = 379
			};
		}
	}

	void run_scaleform_spike_state_checks(Runner& runner)
	{
		using namespace DearModdingUI::MCM::ScaleformSpike;

		runner.test("Scaleform context gate ignores raw PauseMode metadata", [] {
			for (const auto raw : {
					 "not observed",
					 "path lookup failed",
					 "type=UInt value=1",
					 "type=Boolean value=false",
					 "type=undefined value=undefined" })
			{
				auto observation = CompleteContext();
				observation.pauseModePath = raw;
				observation.pauseModeMember = "member lookup failed";
				require(ContextGatePassed(observation),
					"raw PauseMode metadata rejected an otherwise complete context");
			}
		});

		runner.test("Scaleform context gate requires native services and advancement", [] {
			constexpr std::array fields{
				&MovieObservation::root,
				&MovieObservation::f4se,
				&MovieObservation::mcm,
				&MovieObservation::menu,
				&MovieObservation::bgsCodeObject,
				&MovieObservation::setBackgroundVisible,
				&MovieObservation::mcmLoaderContent,
				&MovieObservation::mcmDocumentMenu,
				&MovieObservation::mcmCodeObject,
				&MovieObservation::mcmVersionMethod
			};
			for (const auto field : fields)
			{
				for (const auto missing : { Fact::kMissing, Fact::kNotObserved })
				{
					auto observation = CompleteContext();
					observation.*field = missing;
					require(!ContextGatePassed(observation),
						"a missing native-context requirement was ignored");
				}
			}
			for (const auto call : {
					 CallFact::kNotAttempted, CallFact::kMethodMissing, CallFact::kFailed })
			{
				auto observation = CompleteContext();
				observation.mcmVersionCall = call;
				require(!ContextGatePassed(observation),
					"native callback failure passed the context gate");
			}
			auto observation = CompleteContext();
			observation.currentTimerMs = observation.initialTimerMs;
			require(!ContextGatePassed(observation),
				"a single snapshot was treated as proof of advancement");
			observation.currentFrame = 1;
			require(ContextGatePassed(observation),
				"frame advancement did not satisfy the context gate");
			observation.timingInitialized = false;
			require(!ContextGatePassed(observation),
				"uninitialized timing passed the context gate");
			observation = CompleteContext();
			observation.setBackgroundVisible = Fact::kMissing;
			observation.mcmVersionMethod = Fact::kMissing;
			observation.mcmVersionCall = CallFact::kMethodMissing;
			observation.pauseModePath = "path lookup failed";
			const auto reason = MissingContextReason(observation);
			require(!ContextGatePassed(observation) &&
					reason.find("BGSCodeObj.SetBackgroundVisible") != std::string::npos &&
					reason.find("GetMCMVersionCode") != std::string::npos &&
					reason.find("native callback dispatch") != std::string::npos &&
					reason.find("PauseMode") == std::string::npos &&
					reason.find("advancement") == std::string::npos,
				"failure explanation disagreed with the calibrated context gate");
		});

		runner.test("Scaleform spike no-resource lifecycle rejects duplicate start and settles timeout", [] {
			RunState state;
			const auto first = state.RequestStart();
			const auto duplicate = state.RequestStart();
			require(first.accepted, "first start should be accepted");
			require(!duplicate.accepted, "duplicate start should be rejected");
			require(
				duplicate.generation == first.generation,
				"duplicate start should not advance the generation");
			require(state.BeginLoad(first.generation), "load should begin");
			require(
				state.BeginTerminal(first.generation, Terminal::kTimedOut),
				"timeout should be accepted");
			require(
				state.CurrentPhase() == Phase::kTimedOut,
				"timeout should be terminal when nothing is retained");
			require(!state.NeedsOwnerTick(), "terminal timeout should not tick");
		});

		runner.test("Scaleform spike cancellation invalidates queued work", [] {
			RunState state;
			const auto start = state.RequestStart();
			require(state.BeginLoad(start.generation), "load should begin");
			require(
				state.AcquireResource(start.generation),
				"resource acquisition should be recorded");
			require(state.MarkRunning(start.generation), "run should start");
			const auto stop = state.Cancel();
			require(stop.has_value(), "running probe should be cancellable");
			require(
				!state.BeginTerminal(start.generation, Terminal::kTimedOut),
				"stale queued tick should not change the cancelled run");
			require(
				state.CurrentPhase() == Phase::kReleasing,
				"cancelling a retained resource should begin teardown");
		});

		runner.test("Scaleform spike release cannot complete prematurely", [] {
			RunState state;
			const auto start = state.RequestStart();
			require(state.BeginLoad(start.generation), "load should begin");
			require(
				state.AcquireResource(start.generation),
				"resource acquisition should be recorded");
			require(state.MarkRunning(start.generation), "run should start");
			require(
				state.BeginTerminal(start.generation, Terminal::kStopped),
				"teardown should begin");
			require(
				state.CurrentPhase() == Phase::kReleasing,
				"retained resource should enter releasing");
			require(
				state.ResourceRetained(),
				"resource should remain retained during shutdown");
			require(
				state.MarkCleanupBlocked(start.generation),
				"cleanup wait should be reportable");
			require(
				state.CurrentPhase() == Phase::kCleanupBlocked,
				"blocked cleanup must not report stopped");
			require(
				state.CompleteRelease(start.generation),
				"release should complete only when explicitly confirmed");
			require(
				state.CurrentPhase() == Phase::kStopped &&
					!state.ResourceRetained(),
				"completed release should reach the requested terminal state");
			require(
				!state.CompleteRelease(start.generation),
				"release completion should not be accepted twice");
		});

		runner.test("Scaleform spike verdict survives cancellation during cleanup", [] {
			for (const auto terminal : {
					 Terminal::kPassed,
					 Terminal::kBlocked,
					 Terminal::kTimedOut })
			{
				RunState state;
				const auto start = state.RequestStart();
				require(state.BeginLoad(start.generation), "load should begin");
				require(state.AcquireResource(start.generation), "resource should be retained");
				require(state.MarkRunning(start.generation), "run should start");
				require(state.BeginTerminal(start.generation, terminal), "verdict should settle");
				require(!state.BeginTerminal(start.generation, Terminal::kStopped),
					"a second terminal decision replaced the settled verdict");
				const auto stop = state.Cancel();
				require(stop && state.PendingTerminal() == terminal &&
						state.CurrentPhase() == Phase::kReleasing,
					"stop during shutdown erased the context verdict");
				require(state.MarkCleanupBlocked(*stop), "cleanup should become blocked");
				const auto transition = state.Cancel();
				require(transition && state.PendingTerminal() == terminal &&
						state.CurrentPhase() == Phase::kCleanupBlocked,
					"game transition erased a verdict during blocked cleanup");
				require(!state.CompleteRelease(*stop) &&
						state.CompleteRelease(*transition),
					"cleanup accepted a stale completion or rejected its owner");
				const auto expected = terminal == Terminal::kPassed ?
					Phase::kPassed :
					(terminal == Terminal::kBlocked ? Phase::kBlocked : Phase::kTimedOut);
				require(state.CurrentPhase() == expected && !state.ResourceRetained(),
					"completed cleanup lost the original context verdict");
			}
		});
	}
}
