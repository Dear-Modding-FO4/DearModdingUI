#include "Harness.h"

#include <Support/coalesced-task.h>

#include <deque>
#include <functional>
#include <new>

namespace vmm_tests
{
	void run_coalesced_task_checks(Runner& runner)
	{
		using Addictol::Support::CoalescedTask;
		runner.test("coalesced tasks bound queued and executing work without self-requeueing", [] {
			CoalescedTask task;
			std::deque<std::function<void()>> queue;
			auto submit = [&](auto work) { queue.emplace_back(std::move(work)); };
			unsigned polls{};
			bool acceptedDuringPoll{};
			auto poll = [&]() noexcept {
				++polls;
				acceptedDuringPoll = task.TrySubmit(submit, []() noexcept {});
			};
			require(task.TrySubmit(submit, poll), "initial task was rejected");
			for (unsigned i = 0; i < 20; ++i)
				require(!task.TrySubmit(submit, poll), "duplicate work was accepted");
			require(queue.size() == 1 && polls == 0, "submission ran work or grew the queue");
			auto work = std::move(queue.front());
			queue.pop_front();
			work();
			require(polls == 1 && !acceptedDuringPoll && queue.empty(),
				"execution queued more work into the same drain");
			require(task.TrySubmit(submit, poll), "the next timer tick was rejected");
			queue.front()();
			queue.pop_front();
			require(polls == 2, "subsequent polling stopped");
		});

		runner.test("failed task submission releases the pending slot for retry", [] {
			CoalescedTask task;
			bool threw{};
			try
			{
				(void)task.TrySubmit([](auto) { throw std::bad_alloc{}; }, []() noexcept {});
			}
			catch (const std::bad_alloc&) { threw = true; }
			require(threw, "submission failure was swallowed");
			std::function<void()> work;
			unsigned polls{};
			require(task.TrySubmit(
				[&](auto callback) { work = std::move(callback); },
				[&]() noexcept { ++polls; }), "failed submission held the pending slot");
			work();
			require(polls == 1, "retried work did not execute");
		});
	}
}
