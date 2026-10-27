#include "PlatformImGuiInternal.h"

#include <DearModdingUI/host/Host.h>
#include <Support/ProcessLifetime.h>
#include <Support/Runtime.h>

#include <REX/REX.h>

#include <memory>

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

	bool PlatformImGui::InstallHooks(Callbacks a_callbacks) noexcept
	{
		using namespace PlatformImGuiDetail;
		auto& context = Context();
		auto expected = PlatformImGui::InstallState::kNotAttempted;
		if (!context.installState.compare_exchange_strong(
				expected,
				PlatformImGui::InstallState::kAttempted,
				std::memory_order_acq_rel))
			return PlatformImGui::IsInstalled(expected);

		if (!a_callbacks.Valid())
		{
			context.installState.store(
				PlatformImGui::InstallState::kRejected,
				std::memory_order_release);
			REX::ERROR(
				"[dmui.render.reconciliation] Platform Imgui: required callback bundle is incomplete"sv);
			DearModdingUI::FailBackendInitialization();
			return false;
		}

		context.callbacks = a_callbacks;
		if (!InstallGameCursorHook() || !InstallRendererReconciliation())
		{
			context.installState.store(
				PlatformImGui::InstallState::kRejected,
				std::memory_order_release);
			DearModdingUI::FailBackendInitialization();
			return false;
		}

		context.installState.store(
			PlatformImGui::InstallState::kInstalled,
			std::memory_order_release);
		REX::INFO(
			"[dmui.render.reconciliation] Platform Imgui: renderer reconciliation installed"sv);
		return true;
	}

	bool PlatformImGui::InitializeWindow() noexcept
	{
		using namespace PlatformImGuiDetail;
		if (!PlatformImGui::IsInstalled(
				Context().installState.load(std::memory_order_acquire)))
		{
			REX::ERROR(
				"[dmui.render.reconciliation] Platform Imgui: renderer reconciliation was not installed"sv);
			return false;
		}
		return InitializeRendererReconciliation();
	}

	PlatformImGui::AttachmentResult PlatformImGui::AttachSwapChain(
		IDXGISwapChain* a_swapChain) noexcept
	{
		return PlatformImGuiDetail::AttachExplicitSwapChain(a_swapChain);
	}

	void PlatformImGui::SetDrawingEnabled(bool a_enabled) noexcept
	{
		using namespace PlatformImGuiDetail;
		const ContextLock lock;
		auto& context = Context();
		const auto enable = a_enabled &&
			PlatformImGui::IsInstalled(
				context.installState.load(std::memory_order_acquire)) &&
			context.activeSwapChain.load(std::memory_order_acquire) != nullptr &&
			context.windowReady.load(std::memory_order_acquire) &&
			context.backend.load(std::memory_order_acquire) == Backend::kReady;
		ApplyDrawingRequestLocked(enable);
	}

	void PlatformImGui::HandleGameTransition() noexcept
	{
		using namespace PlatformImGuiDetail;
		const ContextLock lock;
		CloseModalStateLocked(
			DearModdingUI::CarrierMenu::Event::kGameTransition);
	}

	bool PlatformImGui::IsReady() noexcept
	{
		using namespace PlatformImGuiDetail;
		auto& context = Context();
		return PlatformImGui::IsInstalled(
				context.installState.load(std::memory_order_acquire)) &&
			context.activeSwapChain.load(std::memory_order_acquire) != nullptr &&
			context.windowReady.load(std::memory_order_acquire) &&
			context.backend.load(std::memory_order_acquire) == Backend::kReady;
	}

	bool PlatformImGui::QueryVideoMemory(
		uint64_t& a_used,
		uint64_t& a_budget) noexcept
	{
		using namespace PlatformImGuiDetail;
		const ContextLock lock;
		const auto& adapter = Context().attachment.videoMemoryAdapter;
		if (!adapter)
			return false;

		DXGI_QUERY_VIDEO_MEMORY_INFO info{};
		if (FAILED(adapter->QueryVideoMemoryInfo(
				0,
				DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
				std::addressof(info))))
			return false;
		a_used = info.CurrentUsage;
		a_budget = info.Budget;
		return true;
	}
}
