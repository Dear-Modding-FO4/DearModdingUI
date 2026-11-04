#pragma once

#include <DearModdingUI/host/Hotkeys.h>
#include <Platform/menus/HostMenu.h>
#include <Platform/rendering/PlatformImGui.h>
#include <Platform/rendering/FrameSubmission.h>

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#undef ERROR

struct ImGuiIO;

namespace DearModdingUI::PlatformImGuiDetail
{
	enum class Backend : uint32_t
	{
		kUninitialized,
		kReady,
		kFailed
	};

	struct Attachment
	{
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		Microsoft::WRL::ComPtr<IDXGIAdapter3> videoMemoryAdapter;
		HWND window{ nullptr };

		[[nodiscard]] PlatformImGui::AttachmentIdentity Identity() const noexcept
		{
			return {
				reinterpret_cast<uintptr_t>(swapChain.Get()),
				reinterpret_cast<uintptr_t>(device.Get()),
				reinterpret_cast<uintptr_t>(context.Get()),
				reinterpret_cast<uintptr_t>(window)
			};
		}
	};

	// ContextLock owns attachment mutation. installState release-publishes the
	// immutable callback bundle to the render and window threads.
	struct PlatformContext
	{
		Attachment attachment;
		uint64_t attachmentGeneration{ 1 };
		PlatformImGui::AttachmentLifecycle attachmentLifecycle{
			PlatformImGui::AttachmentLifecycle::kVacant
		};
		PlatformImGui::AttachmentSource attachmentSource{
			PlatformImGui::AttachmentSource::kRenderer
		};
		PlatformImGui::RendererProbe reconciledProbe{};
		PlatformImGui::Callbacks callbacks{};
		std::atomic<PlatformImGui::InstallState> installState{
			PlatformImGui::InstallState::kNotAttempted
		};
		std::atomic<IDXGISwapChain*> activeSwapChain{ nullptr };
		std::atomic<HWND> activeWindow{ nullptr };
		std::atomic<bool> drawingEnabled{ false };
		std::atomic<bool> gameLoaded{ false };
		// Toggles polled off the UI thread, applied by the frame thread.
		std::atomic<uint32_t> pendingToggles{ 0 };
		std::atomic<Backend> backend{ Backend::kUninitialized };
		PlatformImGui::FrameSubmission frameSubmission;
	};

	[[nodiscard]] PlatformContext& Context() noexcept;

	// Lock order is renderer data lock -> ContextLock; Locked functions require it.
	class ContextLock
	{
	public:
		ContextLock() noexcept;
		~ContextLock() noexcept;

		ContextLock(const ContextLock&) = delete;
		ContextLock(ContextLock&&) = delete;
		ContextLock& operator=(const ContextLock&) = delete;
		ContextLock& operator=(ContextLock&&) = delete;
	};

	[[nodiscard]] bool InstallRendererReconciliation() noexcept;
	[[nodiscard]] bool InitializeRendererReconciliation() noexcept;
	[[nodiscard]] PlatformImGui::AttachmentResult AttachExplicitSwapChain(
		IDXGISwapChain* a_swapChain) noexcept;
	void PollRendererReconciliation() noexcept;
	void RequestRendererReconciliation() noexcept;
	void RetireActiveAttachmentLocked(
		IDXGISwapChain* a_swapChain,
		HWND a_window) noexcept;
	void SetRendererReadyLocked() noexcept;

	void DrawFrameLocked(IDXGISwapChain* a_swapChain) noexcept;
	// The pointer in backbuffer space, from the native cursor when it is bound.
	[[nodiscard]] PlatformImGui::MousePosition SamplePointerLocked() noexcept;
	[[nodiscard]] bool InstallGameCursorHook() noexcept;
	void DrawBeforeGameCursorLocked(PlatformImGui::MousePosition a_position) noexcept;
	void NoteGameCursorUnavailableLocked(std::string_view a_reason) noexcept;
	void ResetGameCursorWaitLocked() noexcept;
	void ReleaseBackBufferLocked() noexcept;
	void ResetBackBufferFailureLocked() noexcept;
	void ShutdownBackendLocked() noexcept;

	void RefreshInputLanguageLocked(HWND a_window, ImGuiIO& a_io) noexcept;

	// Typed text arrives on the window thread and joins the next frame.
	void DrainTextInputLocked() noexcept;
	void ClearTextInput() noexcept;
	void PublishTextCapture(bool a_wantsText) noexcept;

	[[nodiscard]] DearModdingUI::HotkeyContextState CurrentHotkeyContextState() noexcept;
	void RefreshHotkeyContext() noexcept;
	void DrainQueuedToggles() noexcept;
	void SetModalInputStateLocked(bool a_visible) noexcept;
	void ApplyDrawingRequestLocked(bool a_enabled) noexcept;
	void CloseModalStateLocked(
		DearModdingUI::HostMenu::Event a_event) noexcept;
}
