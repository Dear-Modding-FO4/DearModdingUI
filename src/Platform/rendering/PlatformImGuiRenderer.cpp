// CommonLib declarations must precede the Windows SDK macros.
#include <F4SE/API.h>
#include <F4SE/Interfaces.h>
#include <RE/B/BSGraphics.h>
#include <REX/REX.h>

#include "PlatformImGuiInternal.h"
#include "SwapChainHooks.h"

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <Support/CoalescedTask.h>
#include <Support/SubsystemHealth.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <new>
#include <utility>

namespace Addictol::platformImguiDetail
{
	using namespace std::literals;
	using namespace ImguiPlatform;

	namespace
	{
		static_assert(kPresentTestFlag == DXGI_PRESENT_TEST);
		static_assert(kDxgiErrorDeviceRemoved == static_cast<uint32_t>(DXGI_ERROR_DEVICE_REMOVED));
		static_assert(kDxgiErrorDeviceHung == static_cast<uint32_t>(DXGI_ERROR_DEVICE_HUNG));
		static_assert(kDxgiErrorDeviceReset == static_cast<uint32_t>(DXGI_ERROR_DEVICE_RESET));
		static_assert(kDxgiErrorDriverInternal == static_cast<uint32_t>(DXGI_ERROR_DRIVER_INTERNAL_ERROR));

		struct RendererSnapshot
		{
			Attachment attachment;
			RE::BSGraphics::RendererData* rendererData{ nullptr };
			RE::BSGraphics::RendererWindow* rendererWindow{ nullptr };
			IDXGISwapChain* publishedSwapChain{ nullptr };
		};

		class RendererHealthReporter final :
			public DearModdingUI::HealthReporter
		{
		public:
			void Report(DearModdingUI::HealthEvent a_event,
				const DearModdingUI::HealthSnapshot& a_snapshot) noexcept override;
		};

		struct RendererDataLock
		{
			explicit RendererDataLock(RE::BSGraphics::RendererData& a_data) noexcept :
				lock(std::addressof(a_data.rendererLock.criticalSection))
			{
				REX::W32::EnterCriticalSection(lock);
			}

			~RendererDataLock() noexcept
			{
				REX::W32::LeaveCriticalSection(lock);
			}

			RendererDataLock(const RendererDataLock&) = delete;
			RendererDataLock(RendererDataLock&&) = delete;
			RendererDataLock& operator=(const RendererDataLock&) = delete;
			RendererDataLock& operator=(RendererDataLock&&) = delete;

			REX::W32::CRITICAL_SECTION* lock;
		};

		using ReconciliationClock = DearModdingUI::HealthClock;
		constexpr auto kReconciliationInterval = std::chrono::milliseconds(250);
		constexpr auto kReconciliationDeadline = std::chrono::seconds(10);

		std::atomic<bool> s_missingPresentOriginalLogged{ false };
		std::atomic<bool> s_missingResizeOriginalLogged{ false };
		PTP_TIMER s_reconciliationTimer{ nullptr };
		std::atomic<bool> s_reconciliationTimerStarted{ false };
		Support::CoalescedTask s_reconciliationTask;
		RendererHealthReporter s_rendererHealthReporter;
		DearModdingUI::SubsystemHealth s_rendererHealth{
			"dmui.render.reconciliation",
			s_rendererHealthReporter,
			DearModdingUI::HostSubsystemHealthRegistry()
		};

		HRESULT WINAPI HKPresent(IDXGISwapChain*, UINT, UINT) noexcept;
		HRESULT WINAPI HKResizeBuffers(
			IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) noexcept;

		[[nodiscard]] std::string_view DescribeRendererObservation(
			RendererObservation a_observation) noexcept
		{
			switch (a_observation)
			{
			case RendererObservation::kRendererDataMissing:
				return "renderer data is null";
			case RendererObservation::kRendererNotInitialized:
				return "renderer data is not initialized";
			case RendererObservation::kRendererWindowMissing:
				return "the current renderer window is null";
			case RendererObservation::kSwapChainMissing:
				return "the current renderer window has no swapchain";
			case RendererObservation::kDeviceMissing:
				return "renderer data has no D3D11 device";
			case RendererObservation::kContextMissing:
				return "renderer data has no D3D11 device context";
			case RendererObservation::kWindowMissing:
				return "the current renderer window has no HWND";
			case RendererObservation::kBindingChanged:
				return "the renderer binding changed while it was captured";
			case RendererObservation::kInvalidBinding:
				return "the attachment candidate is invalid";
			case RendererObservation::kHookInstallationFailed:
				return "the renderer binding was valid but swapchain hooks could not be installed";
			default:
				return "the renderer binding is valid";
			}
		}

		void RendererHealthReporter::Report(
			DearModdingUI::HealthEvent a_event,
			const DearModdingUI::HealthSnapshot& a_snapshot) noexcept
		{
			using DearModdingUI::HealthEvent;
			using DearModdingUI::HealthState;

			if (a_event == HealthEvent::kDeadlineExceeded)
			{
				if (a_snapshot.state == HealthState::kProgressing)
				{
					REX::ERROR(
						"[dmui.render.reconciliation] Platform Imgui: renderer reconciliation deadline exceeded: the renderer binding is valid but Present has not been observed; reconciliation will continue"sv);
				}
				else
				{
					REX::ERROR(
						"[dmui.render.reconciliation] Platform Imgui: renderer reconciliation deadline exceeded: {}; reconciliation will continue"sv,
						a_snapshot.reason);
				}
				return;
			}

			const auto& attachment = Context().attachment;
			if (a_snapshot.state == HealthState::kWaiting)
			{
				REX::INFO(
					"[dmui.render.reconciliation] Platform Imgui: renderer state: waiting for renderer ({})"sv,
					a_snapshot.reason);
			}
			else if (a_snapshot.state == HealthState::kProgressing)
			{
				if (a_event == HealthEvent::kDeadlineProgress)
				{
					REX::INFO(
						"[dmui.render.reconciliation] Platform Imgui: renderer made progress after its deadline: bound and waiting for Present (swapchain {}, device {}, context {}, window {})"sv,
						static_cast<void*>(attachment.swapChain.Get()),
						static_cast<void*>(attachment.device.Get()),
						static_cast<void*>(attachment.context.Get()),
						static_cast<void*>(attachment.window));
				}
				else
				{
					REX::INFO(
						"[dmui.render.reconciliation] Platform Imgui: renderer state: bound and waiting for Present (swapchain {}, device {}, context {}, window {})"sv,
						static_cast<void*>(attachment.swapChain.Get()),
						static_cast<void*>(attachment.device.Get()),
						static_cast<void*>(attachment.context.Get()),
						static_cast<void*>(attachment.window));
				}
			}
			else if (a_snapshot.state == HealthState::kReady)
			{
				if (a_event == HealthEvent::kDeadlineRecovery)
				{
					REX::INFO(
						"[dmui.render.reconciliation] Platform Imgui: renderer state recovered after deadline: ready"sv);
				}
				else if (a_event == HealthEvent::kRecovery)
				{
					REX::INFO(
						"[dmui.render.reconciliation] Platform Imgui: renderer state recovered: ready"sv);
				}
				else
				{
					REX::INFO(
						"[dmui.render.reconciliation] Platform Imgui: renderer state: ready"sv);
				}
			}
			else if (a_snapshot.state == HealthState::kDegraded)
			{
				REX::WARN(
					"[dmui.render.reconciliation] Platform Imgui: renderer state degraded: {}"sv,
					a_snapshot.reason);
			}
			else if (a_snapshot.state == HealthState::kFailed)
			{
				REX::ERROR(
					"[dmui.render.reconciliation] Platform Imgui: renderer state failed: {}"sv,
					a_snapshot.reason);
			}
		}

		void SetRendererWaitingLocked(
			RendererObservation a_observation) noexcept
		{
			(void)s_rendererHealth.Observe(
				DearModdingUI::HealthState::kWaiting,
				DescribeRendererObservation(a_observation));
		}

		void SetRendererBoundLocked() noexcept
		{
			(void)s_rendererHealth.Observe(
				DearModdingUI::HealthState::kProgressing,
				"the renderer binding is valid but Present has not been observed");
		}

		void AdvanceAttachmentGenerationLocked() noexcept
		{
			auto& generation = Context().attachmentGeneration;
			++generation;
			if (generation == 0)
				generation = 1;
		}

		void LogExecutionTransition(
			const DearModdingUI::RenderExecution::ThreadTransition&
				a_transition) noexcept
		{
			if (!a_transition.changed)
				return;
			const auto phase = a_transition.phase ==
					DearModdingUI::RenderExecution::Phase::kFrameObservation ?
				"observer"sv :
				"draw"sv;
			REX::INFO(
				"Platform Imgui: active Present execution moved threads (old {}, new {}, phase {}, binding {})"sv,
				a_transition.previousThread,
				a_transition.currentThread,
				phase,
				a_transition.bindingId);
		}

		[[nodiscard]] Microsoft::WRL::ComPtr<IDXGIAdapter3> AcquireVideoMemoryAdapter(
			ID3D11Device* a_device) noexcept
		{
			Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
			Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
			Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
			if (FAILED(a_device->QueryInterface(
					IID_PPV_ARGS(dxgiDevice.ReleaseAndGetAddressOf()))) ||
				FAILED(dxgiDevice->GetParent(
					IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) ||
				FAILED(adapter->QueryInterface(
					IID_PPV_ARGS(adapter3.ReleaseAndGetAddressOf()))))
				return {};
			return adapter3;
		}

		[[nodiscard]] bool CaptureRendererSnapshot(
			RendererSnapshot& a_snapshot,
			RendererObservation& a_observation) noexcept
		{
			auto* rendererData = RE::BSGraphics::GetRendererData();
			if (!rendererData)
			{
				a_observation =
					RendererObservation::kRendererDataMissing;
				return false;
			}

			const RendererDataLock rendererLock{ *rendererData };
			if (RE::BSGraphics::GetRendererData() != rendererData)
			{
				a_observation =
					RendererObservation::kBindingChanged;
				return false;
			}
			auto* rendererWindow = RE::BSGraphics::GetCurrentRendererWindow();
			const RendererProbe probe{
				true,
				rendererData->initialized,
				rendererWindow != nullptr,
				{
					rendererWindow ? reinterpret_cast<uintptr_t>(
						rendererWindow->swapChain) : 0,
					reinterpret_cast<uintptr_t>(rendererData->device),
					reinterpret_cast<uintptr_t>(rendererData->context),
					rendererWindow ? reinterpret_cast<uintptr_t>(
						rendererWindow->hwnd) : 0
				}
			};
			a_observation = ObserveRenderer(probe);
			if (a_observation != RendererObservation::kReady)
				return false;

			a_snapshot.rendererData = rendererData;
			a_snapshot.rendererWindow = rendererWindow;
			a_snapshot.publishedSwapChain = reinterpret_cast<IDXGISwapChain*>(
				rendererWindow->swapChain);
			a_snapshot.attachment.swapChain =
				a_snapshot.publishedSwapChain;
			a_snapshot.attachment.device =
				reinterpret_cast<ID3D11Device*>(rendererData->device);
			a_snapshot.attachment.context =
				reinterpret_cast<ID3D11DeviceContext*>(rendererData->context);
			a_snapshot.attachment.window = reinterpret_cast<HWND>(
				rendererWindow->hwnd);
			return true;
		}

		[[nodiscard]] bool ValidateRendererSnapshot(
			const RendererSnapshot& a_snapshot) noexcept
		{
			return
				RE::BSGraphics::GetRendererData() ==
					a_snapshot.rendererData &&
				RE::BSGraphics::GetCurrentRendererWindow() ==
					a_snapshot.rendererWindow &&
				a_snapshot.rendererData->initialized &&
				reinterpret_cast<IDXGISwapChain*>(
					a_snapshot.rendererWindow->swapChain) ==
					a_snapshot.publishedSwapChain &&
				reinterpret_cast<ID3D11Device*>(
					a_snapshot.rendererData->device) ==
					a_snapshot.attachment.device.Get() &&
				reinterpret_cast<ID3D11DeviceContext*>(
					a_snapshot.rendererData->context) ==
					a_snapshot.attachment.context.Get() &&
				reinterpret_cast<HWND>(
					a_snapshot.rendererWindow->hwnd) ==
					a_snapshot.attachment.window;
		}

		[[nodiscard]] bool PrepareExplicitSnapshot(
			IDXGISwapChain* a_swapChain,
			RendererSnapshot& a_snapshot,
			RendererObservation& a_observation) noexcept
		{
			if (!a_swapChain)
			{
				a_observation =
					RendererObservation::kInvalidBinding;
				return false;
			}
			if (!CaptureRendererSnapshot(
					a_snapshot,
					a_observation))
				return false;

			a_snapshot.attachment.swapChain = a_swapChain;
			return true;
		}

		void CompleteRendererSnapshot(
			RendererSnapshot& a_snapshot) noexcept
		{
			a_snapshot.attachment.videoMemoryAdapter =
				AcquireVideoMemoryAdapter(
					a_snapshot.attachment.device.Get());
		}

		[[nodiscard]] bool ValidateSnapshotForCommit(
			const RendererSnapshot& a_snapshot,
			RendererObservation& a_observation) noexcept
		{
			if (!ValidateRendererSnapshot(a_snapshot))
			{
				a_observation =
					RendererObservation::kBindingChanged;
				return false;
			}
			return true;
		}

		[[nodiscard]] bool CommitRendererSnapshot(
			RendererSnapshot& a_snapshot,
			AttachmentSource a_source,
			RendererObservation& a_observation) noexcept
		{
			// Keep published renderer identities stable through commit.
			const RendererDataLock rendererLock{ *a_snapshot.rendererData };
			if (!ValidateSnapshotForCommit(a_snapshot, a_observation))
				return false;
			const ContextLock lock;
			if (!ValidateSnapshotForCommit(a_snapshot, a_observation))
				return false;

			auto& context = Context();
			const auto decision = DecideAttachment(
				context.attachment.Identity(),
				a_snapshot.attachment.Identity(),
				context.attachmentSource,
				a_source,
				context.attachmentLifecycle);
			if (decision == AttachmentDecision::kReject)
			{
				a_observation =
					RendererObservation::kInvalidBinding;
				return false;
			}
			if (decision == AttachmentDecision::kKeepCurrent)
				return true;
			if (!SwapChainHooks::Install(
					a_snapshot.attachment.swapChain.Get(),
					context.attachmentLifecycle,
					&HKPresent,
					&HKResizeBuffers))
			{
				a_observation =
					RendererObservation::kHookInstallationFailed;
				return false;
			}

			if (decision == AttachmentDecision::kReplace)
				RetireActiveAttachmentLocked(nullptr, nullptr);

			context.attachment = std::move(a_snapshot.attachment);
			AdvanceAttachmentGenerationLocked();
			context.attachmentLifecycle =
				AttachmentLifecycle::kActive;
			context.attachmentSource = a_source;
			context.activeWindow.store(
				context.attachment.window,
				std::memory_order_release);
			context.windowReady.store(
				HasWindowHook(context.attachment.window),
				std::memory_order_release);
			context.activeSwapChain.store(
				context.attachment.swapChain.Get(),
				std::memory_order_release);
			if (context.gameLoaded.load(
					std::memory_order_acquire) &&
				!context.windowReady.load(
					std::memory_order_acquire) &&
				!SubclassWindowLocked(context.attachment.window))
			{
				REX::WARN(
					"Platform Imgui: active renderer window could not be subclassed before Present"sv);
			}

			SetRendererBoundLocked();
			return true;
		}

		[[nodiscard]] bool ReconcileRenderer() noexcept
		{
			RendererSnapshot snapshot;
			RendererObservation observation{
				RendererObservation::kRendererDataMissing
			};
			if (!CaptureRendererSnapshot(snapshot, observation))
			{
				const ContextLock lock;
				if (!Context().attachment.Identity().Valid())
					SetRendererWaitingLocked(observation);
				return true;
			}
			CompleteRendererSnapshot(snapshot);
			const auto committed = CommitRendererSnapshot(
				snapshot, AttachmentSource::kRenderer, observation);
			if (!committed)
			{
				const ContextLock lock;
				if (!Context().attachment.Identity().Valid())
					SetRendererWaitingLocked(observation);
			}
			return committed ||
				observation ==
					RendererObservation::kBindingChanged;
		}

		void CheckReconciliationDeadline() noexcept
		{
			if (!Context().gameLoaded.load(
					std::memory_order_acquire))
				return;

			const ContextLock lock;
			if (Context().backend.load(
					std::memory_order_acquire) ==
				Backend::kFailed)
				return;
			s_rendererHealth.Evaluate();
		}

		void CALLBACK QueueRendererReconciliation(
			PTP_CALLBACK_INSTANCE,
			void*,
			PTP_TIMER) noexcept
		{
			static std::atomic<bool> failureReported{ false };
			try
			{
				const auto submitted =
					s_reconciliationTask.TrySubmit(
						[](auto a_work) {
							struct Task final :
								F4SE::ITaskDelegate
							{
								explicit Task(
									decltype(a_work)
										a_callback) noexcept :
									callback(std::move(
										a_callback))
								{}

								void Run() override
								{
									callback();
								}

								decltype(a_work) callback;
							};
							auto task =
								std::make_unique<Task>(
									std::move(a_work));
							F4SE::GetTaskInterface()->AddTask(
								task.get());
							(void)task.release();
						},
						&PollRendererReconciliation);
				if (submitted &&
					failureReported.exchange(false))
				{
					REX::INFO(
						"[dmui.render.reconciliation] Main-thread task submission recovered"sv);
				}
			}
			catch (const std::bad_alloc&)
			{
				if (!failureReported.exchange(true))
				{
					REX::ERROR(
						"[dmui.render.reconciliation] Could not allocate renderer task; will retry"sv);
				}
			}
		}

		HRESULT WINAPI HKPresent(
			IDXGISwapChain* a_swapChain,
			UINT a_syncInterval,
			UINT a_flags) noexcept
		{
			const auto original =
				SwapChainHooks::PreviousPresent(a_swapChain, &HKPresent);
			if (!original)
			{
				if (!s_missingPresentOriginalLogged.exchange(
						true,
						std::memory_order_acq_rel))
				{
					REX::ERROR(
						"Platform Imgui: Present hook has no previous target"sv);
				}
				return DXGI_ERROR_INVALID_CALL;
			}

			PresentAttachmentToken presented{};
			auto& context = Context();
			if (a_swapChain ==
				context.activeSwapChain.load(
					std::memory_order_acquire))
			{
				if ((a_flags & DXGI_PRESENT_TEST) != 0)
				{
					const ContextLock lock;
					if (a_swapChain ==
							context.attachment.swapChain.Get() &&
						context.attachmentLifecycle ==
							AttachmentLifecycle::kActive)
					{
						presented = {
							reinterpret_cast<uintptr_t>(
								a_swapChain),
							context.attachmentGeneration
						};
					}
				}
				else
				{
					DearModdingUI::RenderExecution::Guard
						execution{
							DearModdingUI::RenderExecution::
								Phase::kFrameDraw
						};
					const ContextLock lock;
					if (a_swapChain ==
							context.attachment.swapChain.Get() &&
						context.attachmentLifecycle ==
							AttachmentLifecycle::kActive)
					{
						presented = {
							reinterpret_cast<uintptr_t>(
								a_swapChain),
							context.attachmentGeneration
						};
						LogExecutionTransition(
							execution.NoteBinding(
								context.
									attachmentGeneration));
						const auto activeWindow =
							context.activeWindow.load(
								std::memory_order_acquire);
						if (activeWindow &&
							activeWindow ==
								context.attachment.window &&
							context.gameLoaded.load(
								std::memory_order_acquire) &&
							!context.windowReady.load(
								std::memory_order_acquire))
						{
							if (!SubclassWindowLocked(
									context.
										attachment.window))
							{
								CloseModalStateLocked(
									DearModdingUI::CarrierMenu::Event::
										kBackendFailure);
								DearModdingUI::
									FailBackendInitialization();
							}
						}
						DrawFrameLocked(a_swapChain);
					}
				}
			}

			const auto result = original(
				a_swapChain,
				a_syncInterval,
				a_flags);
			if (presented.Valid() &&
				ObservesDisplayedFrame(
					a_flags,
					result == S_OK))
			{
				DearModdingUI::RenderExecution::Guard execution{
					DearModdingUI::RenderExecution::Phase::
						kFrameObservation
				};
				bool observe{ false };
				{
					const ContextLock lock;
					observe = MatchesActivePresentAttachment(
						presented,
						reinterpret_cast<uintptr_t>(
							context.
								attachment.swapChain.Get()),
						context.attachmentGeneration,
						context.attachmentLifecycle);
					if (observe)
					{
						LogExecutionTransition(
							execution.NoteBinding(
								context.
									attachmentGeneration));
					}
				}
				if (observe)
					DearModdingUI::ObserveFrame();
			}
			if (presented.Valid() &&
				IsDefinitiveSwapChainLoss(
					static_cast<uint32_t>(result)))
			{
				const ContextLock lock;
				if (MatchesActivePresentAttachment(
						presented,
						reinterpret_cast<uintptr_t>(
							context.
								attachment.swapChain.Get()),
						context.attachmentGeneration,
						context.attachmentLifecycle))
				{
					RetireActiveAttachmentLocked(
						a_swapChain,
						nullptr);
				}
			}
			return result;
		}

		HRESULT WINAPI HKResizeBuffers(
			IDXGISwapChain* a_swapChain,
			UINT a_bufferCount,
			UINT a_width,
			UINT a_height,
			DXGI_FORMAT a_format,
			UINT a_flags) noexcept
		{
			const auto original = SwapChainHooks::PreviousResizeBuffers(
				a_swapChain,
				&HKResizeBuffers);
			auto& context = Context();
			if (a_swapChain ==
				context.activeSwapChain.load(
					std::memory_order_acquire))
			{
				const ContextLock lock;
				if (a_swapChain ==
					context.attachment.swapChain.Get())
				{
					ReleaseBackBufferLocked();
					ResetBackBufferFailureLocked();
				}
			}

			if (original)
			{
				const auto result = original(
					a_swapChain,
					a_bufferCount,
					a_width,
					a_height,
					a_format,
					a_flags);
				if (IsDefinitiveSwapChainLoss(
						static_cast<uint32_t>(result)))
				{
					const ContextLock lock;
					RetireActiveAttachmentLocked(
						a_swapChain,
						nullptr);
				}
				return result;
			}
			if (!s_missingResizeOriginalLogged.exchange(
					true,
					std::memory_order_acq_rel))
			{
				REX::ERROR(
					"Platform Imgui: ResizeBuffers hook has no previous target"sv);
			}
			return DXGI_ERROR_INVALID_CALL;
		}
	}

	void SetRendererReadyLocked() noexcept
	{
		(void)s_rendererHealth.Observe(
			DearModdingUI::HealthState::kReady,
			{});
	}

	void RequestRendererReconciliation() noexcept
	{
		if (!s_reconciliationTimerStarted.load(
				std::memory_order_acquire))
			return;
		FILETIME due{};
		SetThreadpoolTimer(
			s_reconciliationTimer,
			std::addressof(due),
			static_cast<DWORD>(
				kReconciliationInterval.count()),
			0);
	}

	void RetireActiveAttachmentLocked(
		IDXGISwapChain* a_swapChain,
		HWND a_window) noexcept
	{
		auto& context = Context();
		if ((a_swapChain &&
				context.attachment.swapChain.Get() != a_swapChain) ||
			(a_window &&
				context.attachment.window != a_window))
			return;

		CloseModalStateLocked(
			DearModdingUI::CarrierMenu::Event::kRetarget);
		ReleaseBackBufferLocked();
		ResetBackBufferFailureLocked();
		ShutdownBackendLocked();
		context.activeSwapChain.store(
			nullptr,
			std::memory_order_release);
		context.activeWindow.store(
			nullptr,
			std::memory_order_release);
		context.windowReady.store(
			false,
			std::memory_order_release);
		context.attachment = {};
		AdvanceAttachmentGenerationLocked();
		context.attachmentLifecycle =
			AttachmentLifecycle::kRetired;
		context.attachmentSource =
			AttachmentSource::kRenderer;
		s_rendererHealth.InvalidateObservation();
		ClearConsumedToggleKeysLocked();
		RequestRendererReconciliation();
	}

	void PollRendererReconciliation() noexcept
	{
		if (!Context().gameLoaded.load(
				std::memory_order_acquire))
			return;
		(void)ReconcileRenderer();
		CheckReconciliationDeadline();
	}

	bool InstallRendererReconciliation() noexcept
	{
		if (!F4SE::GetTaskInterface())
		{
			REX::ERROR(
				"[dmui.render.reconciliation] Platform Imgui: renderer reconciliation task interface is unavailable"sv);
			return false;
		}

		// Like the render hooks, the armed timer lives until process exit.
		s_reconciliationTimer = CreateThreadpoolTimer(
			&QueueRendererReconciliation,
			nullptr,
			nullptr);
		if (!s_reconciliationTimer)
		{
			REX::ERROR(
				"[dmui.render.reconciliation] Could not create renderer timer (Windows error {})"sv,
				GetLastError());
			return false;
		}
		return true;
	}

	bool InitializeRendererReconciliation() noexcept
	{
		if (!s_reconciliationTimer)
		{
			REX::ERROR(
				"[dmui.render.reconciliation] Platform Imgui: renderer reconciliation was not installed"sv);
			return false;
		}

		Context().gameLoaded.store(true, std::memory_order_release);
		{
			const ContextLock lock;
			s_rendererHealth.SetDeadline(
				ReconciliationClock::now() + kReconciliationDeadline);
		}
		PollRendererReconciliation();
		if (!s_reconciliationTimerStarted.exchange(
				true,
				std::memory_order_acq_rel))
		{
			const auto ticks =
				-kReconciliationInterval.count() * 10000;
			FILETIME due{
				static_cast<DWORD>(ticks),
				static_cast<DWORD>(
					static_cast<uint64_t>(ticks) >> 32)
			};
			SetThreadpoolTimer(
				s_reconciliationTimer,
				std::addressof(due),
				static_cast<DWORD>(
					kReconciliationInterval.count()),
				0);
		}
		return true;
	}

	ImguiPlatform::AttachmentResult AttachExplicitSwapChain(
		IDXGISwapChain* a_swapChain) noexcept
	{
		RendererSnapshot snapshot;
		RendererObservation observation{
			RendererObservation::kRendererDataMissing
		};
		if (!PrepareExplicitSnapshot(
				a_swapChain,
				snapshot,
				observation))
		{
			REX::WARN(
				"Platform Imgui: explicit swapchain override not attached ({})"sv,
				DescribeRendererObservation(observation));
			return FailedAttachmentResult(observation);
		}
		CompleteRendererSnapshot(snapshot);
		const auto committed = CommitRendererSnapshot(
			snapshot,
			AttachmentSource::kExplicit,
			observation);
		if (!committed)
		{
			REX::WARN(
				"Platform Imgui: explicit swapchain override could not be committed ({})"sv,
				DescribeRendererObservation(observation));
		}
		return committed ?
			AttachmentResult::kAttached :
			FailedAttachmentResult(observation);
	}
}
