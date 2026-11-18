// CommonLib declarations must precede the Windows SDK macros.
#include <RE/C/CursorMenu.h>
#include <RE/U/UI.h>

#include "PlatformImGuiInternal.h"

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <Support/ProcessLifetime.h>

#include <REX/REX.h>

namespace DearModdingUI
{
	using namespace std::literals;

	namespace PlatformImGuiDetail
	{
		namespace
		{
			Support::ProcessLifetime<PlatformContext> s_context;
			INIT_ONCE s_contextLockOnce = INIT_ONCE_STATIC_INIT;
			CRITICAL_SECTION s_contextLock{};

			BOOL CALLBACK InitializeContextLock(
				[[maybe_unused]] PINIT_ONCE a_once,
				[[maybe_unused]] PVOID a_parameter,
				[[maybe_unused]] PVOID* a_context) noexcept
			{
				InitializeCriticalSection(std::addressof(s_contextLock));
				return TRUE;
			}
		}

		PlatformContext& Context() noexcept
		{
			return s_context.value;
		}

		ContextLock::ContextLock() noexcept
		{
			InitOnceExecuteOnce(
				std::addressof(s_contextLockOnce),
				InitializeContextLock,
				nullptr,
				nullptr);
			EnterCriticalSection(std::addressof(s_contextLock));
		}

		ContextLock::~ContextLock() noexcept
		{
			LeaveCriticalSection(std::addressof(s_contextLock));
		}
	}

	bool PlatformImGui::Initialize(Callbacks a_callbacks) noexcept
	{
		using namespace PlatformImGuiDetail;
		auto& context = Context();
		if (context.initialized.load(std::memory_order_acquire))
			return true;
		if (!a_callbacks.Valid())
		{
			REX::ERROR("Platform Imgui: required callback bundle is incomplete"sv);
			DearModdingUI::FailBackendInitialization();
			return false;
		}
		if (!InstallGameCursorHook())
		{
			DearModdingUI::FailBackendInitialization();
			return false;
		}
		context.callbacks = a_callbacks;
		InitializeRendererHealth();
		context.initialized.store(true, std::memory_order_release);
		return true;
	}

	namespace
	{
		// The engine itself checks menu state this way inside the same pass.
		[[nodiscard]] bool GameCursorOpen() noexcept
		{
			auto* ui = RE::UI::GetSingleton();
			static const RE::BSFixedString cursorMenu{ RE::CursorMenu::MENU_NAME };
			return ui && ui->GetMenuOpen(cursorMenu);
		}
	}

	void PlatformImGui::DrawUiPass(UiPassSite a_site) noexcept
	{
		using namespace PlatformImGuiDetail;
		auto& context = Context();
		if (!context.initialized.load(std::memory_order_acquire))
			return;
		// The frame menu is asked before the cursor, so one site draws.
		if (a_site == UiPassSite::kFrameMenu && GameCursorOpen())
			return;
		if (!context.uiPassObserved.exchange(true, std::memory_order_acq_rel))
			REX::INFO("Platform Imgui: the game's UI pass reached the host on thread {}"sv, GetCurrentThreadId());

		// Toggle callbacks take the context lock themselves.
		DrainQueuedToggles();
		{
			const DearModdingUI::RenderExecution::Guard execution{
				DearModdingUI::RenderExecution::Phase::kFrameObservation
			};
			DearModdingUI::ObserveFrame();
		}
		const DearModdingUI::RenderExecution::Guard execution{
			DearModdingUI::RenderExecution::Phase::kFrameDraw
		};
		const ContextLock lock;
		DrawUiPassLocked();
	}

	void PlatformImGui::HandleFrameMenuClosed() noexcept
	{
		using namespace PlatformImGuiDetail;
		const ContextLock lock;
		CloseModalStateLocked(DearModdingUI::HostMenu::Event::kClose);
	}

	void PlatformImGui::NoteGameDataThread() noexcept
	{
		using namespace PlatformImGuiDetail;
		auto* const thread = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
		if (!thread)
		{
			REX::ERROR("Platform Imgui: the game-data thread could not be opened (Windows error {})"sv, GetLastError());
			return;
		}
		HANDLE expected{ nullptr };
		if (!Context().gameDataThread.compare_exchange_strong(expected, thread, std::memory_order_acq_rel))
			CloseHandle(thread);
	}

	void PlatformImGui::SetDrawingEnabled(bool a_enabled) noexcept
	{
		using namespace PlatformImGuiDetail;
		const ContextLock lock;
		ApplyDrawingRequestLocked(a_enabled && IsReady());
	}

	void PlatformImGui::HandleGameTransition() noexcept
	{
		using namespace PlatformImGuiDetail;
		const ContextLock lock;
		CloseModalStateLocked(
			DearModdingUI::HostMenu::Event::kGameTransition);
	}

	PlatformImGui::Readiness PlatformImGui::CurrentReadiness() noexcept
	{
		using namespace PlatformImGuiDetail;
		auto& context = Context();
		const auto backend = context.backend.load(std::memory_order_acquire);
		return PlatformImGui::ClassifyReadiness(
			context.uiPassObserved.load(std::memory_order_acquire),
			backend == Backend::kReady,
			backend == Backend::kFailed);
	}

	bool PlatformImGui::IsReady() noexcept
	{
		return CurrentReadiness() == Readiness::kReady;
	}

	bool PlatformImGui::QueryVideoMemory(
		uint64_t& a_used,
		uint64_t& a_budget) noexcept
	{
		using namespace PlatformImGuiDetail;
		const ContextLock lock;
		return QueryVideoMemoryLocked(a_used, a_budget);
	}
}
