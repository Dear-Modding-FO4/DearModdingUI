#include <DearModdingUI/host/RenderExecution.h>

#include <mutex>

namespace DearModdingUI::RenderExecution
{
	namespace
	{
		[[nodiscard]] std::recursive_mutex& Gate() noexcept
		{
			static std::recursive_mutex gate;
			return gate;
		}

		thread_local uint32_t s_depth{};
		thread_local Phase s_phase{ Phase::kFrameDraw };
		thread_local DMUI_ClientHandle s_activeClient{
			DMUI_INVALID_CLIENT_HANDLE
		};
		thread_local bool s_drawingCallback{};
		thread_local DMUI_PageHandle s_activePage{};
	}

	Guard::Guard(Phase a_phase) noexcept :
		m_previousPhase(s_phase)
	{
		Gate().lock();
		++s_depth;
		s_phase = a_phase;
	}

	Guard::~Guard() noexcept
	{
		s_phase = m_previousPhase;
		--s_depth;
		Gate().unlock();
	}

	ClientGuard::ClientGuard(
		DMUI_ClientHandle a_client,
		bool a_drawing, DMUI_PageHandle a_page) noexcept :
		m_previousClient(s_activeClient),
		m_previousDrawing(s_drawingCallback),
		m_previousPage(s_activePage)
	{
		s_activeClient = a_client;
		s_drawingCallback = a_drawing;
		s_activePage = a_page;
	}

	ClientGuard::~ClientGuard() noexcept
	{
		s_activeClient = m_previousClient;
		s_drawingCallback = m_previousDrawing;
		s_activePage = m_previousPage;
	}

	bool IsActive() noexcept
	{
		return s_depth != 0;
	}

	bool IsActiveClient(
		DMUI_ClientHandle a_client,
		bool a_drawingRequired) noexcept
	{
		return IsActive() &&
			s_activeClient == a_client &&
			(!a_drawingRequired || s_drawingCallback);
	}

	Phase ActivePhase() noexcept
	{
		return s_phase;
	}

	DMUI_PageHandle ActivePage() noexcept { return s_activePage; }
}
