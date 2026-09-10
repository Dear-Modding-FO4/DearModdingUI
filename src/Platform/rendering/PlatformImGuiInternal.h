#pragma once

#include <Platform/input/CarrierMenu.h>
#include <Platform/rendering/PlatformImGui.h>

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

#undef ERROR

namespace Addictol::platformImguiDetail
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

		[[nodiscard]] ImguiPlatform::AttachmentIdentity Identity() const noexcept
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
		ImguiPlatform::AttachmentLifecycle attachmentLifecycle{
			ImguiPlatform::AttachmentLifecycle::kVacant
		};
		ImguiPlatform::AttachmentSource attachmentSource{
			ImguiPlatform::AttachmentSource::kRenderer
		};
		PlatformImgui::Callbacks callbacks{};
		std::atomic<ImguiPlatform::InstallState> installState{
			ImguiPlatform::InstallState::kNotAttempted
		};
		std::atomic<IDXGISwapChain*> activeSwapChain{ nullptr };
		std::atomic<HWND> activeWindow{ nullptr };
		std::atomic<bool> drawingEnabled{ false };
		std::atomic<bool> windowReady{ false };
		std::atomic<bool> gameLoaded{ false };
		std::atomic<Backend> backend{ Backend::kUninitialized };
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
	[[nodiscard]] ImguiPlatform::AttachmentResult AttachExplicitSwapChain(
		IDXGISwapChain* a_swapChain) noexcept;
	void PollRendererReconciliation() noexcept;
	void RequestRendererReconciliation() noexcept;
	void RetireActiveAttachmentLocked(
		IDXGISwapChain* a_swapChain,
		HWND a_window) noexcept;
	void SetRendererReadyLocked() noexcept;

	void DrawFrameLocked(IDXGISwapChain* a_swapChain) noexcept;
	void ReleaseBackBufferLocked() noexcept;
	void ResetBackBufferFailureLocked() noexcept;
	void ShutdownBackendLocked() noexcept;

	struct BackendMessageResult
	{
		LRESULT result{ 0 };
		bool handled{ false };
		bool swallow{ false };
	};

	[[nodiscard]] BackendMessageResult HandleBackendWindowMessageLocked(
		HWND a_window,
		UINT a_message,
		WPARAM a_wparam,
		LPARAM a_lparam,
		bool a_escapeConsumed) noexcept;

	[[nodiscard]] bool HasWindowHook(HWND a_window) noexcept;
	[[nodiscard]] bool SubclassWindowLocked(HWND a_window) noexcept;
	void SetModalInputStateLocked(bool a_visible) noexcept;
	void ApplyDrawingRequestLocked(bool a_enabled) noexcept;
	void CloseModalStateLocked(
		DearModdingUI::CarrierMenu::Event a_event) noexcept;
	void ClearConsumedToggleKeysLocked() noexcept;
}
