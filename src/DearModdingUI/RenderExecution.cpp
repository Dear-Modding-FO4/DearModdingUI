#include <DearModdingUI/RenderExecution.h>

#include <Windows.h>

#include <mutex>

namespace DearModdingUI::RenderExecution
{
	namespace
	{
		struct State
		{
			std::recursive_mutex gate;
			uint64_t lastThread{};
		};

		[[nodiscard]] State& GetState() noexcept
		{
			static State state;
			return state;
		}

		[[nodiscard]] uint64_t CurrentThreadToken() noexcept
		{
			return static_cast<uint64_t>(::GetCurrentThreadId());
		}

		thread_local uint32_t s_depth{};
		thread_local Phase s_phase{ Phase::kFrameDraw };
		thread_local uint64_t s_binding{};
	}

	Guard::Guard(Phase a_phase) noexcept :
		m_previousPhase(s_phase),
		m_previousBinding(s_binding),
		m_outermost(s_depth == 0)
	{
		GetState().gate.lock();
		++s_depth;
		s_phase = a_phase;
		s_binding = 0;
	}

	Guard::~Guard() noexcept
	{
		s_phase = m_previousPhase;
		s_binding = m_previousBinding;
		--s_depth;
		GetState().gate.unlock();
	}

	ThreadTransition Guard::NoteBinding(uint64_t a_bindingId) noexcept
	{
		s_binding = a_bindingId;
		ThreadTransition transition{
			0,
			CurrentThreadToken(),
			a_bindingId,
			s_phase,
			false
		};
		if (!m_outermost || m_bindingNoted)
			return transition;

		m_bindingNoted = true;
		auto& state = GetState();
		transition.previousThread = state.lastThread;
		transition.changed =
			state.lastThread != 0 &&
			state.lastThread != transition.currentThread;
		state.lastThread = transition.currentThread;
		return transition;
	}

	bool IsActive() noexcept
	{
		return s_depth != 0;
	}

	Phase ActivePhase() noexcept
	{
		return s_phase;
	}

	uint64_t ActiveBinding() noexcept
	{
		return s_binding;
	}
}
