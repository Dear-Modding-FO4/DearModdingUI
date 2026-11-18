#pragma once

#include <DearModdingUI/host/Hotkeys.h>
#include <Platform/menus/HostMenu.h>
#include <Platform/rendering/PlatformImGui.h>

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <atomic>
#include <cstdint>

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

	// The engine's device, context and window; captured once, kept for the process.
	struct RenderDevice
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		Microsoft::WRL::ComPtr<IDXGIAdapter3> videoMemoryAdapter;
	};

	// ContextLock guards mutation; initialized publishes the callbacks.
	struct PlatformContext
	{
		RenderDevice render;
		PlatformImGui::Callbacks callbacks{};
		std::atomic<bool> initialized{ false };
		std::atomic<bool> uiPassObserved{ false };
		// Its exit ends every kGameDataReady and kGameLoaded handler.
		std::atomic<HANDLE> gameDataThread{ nullptr };
		std::atomic<HWND> window{ nullptr };
		std::atomic<bool> drawingEnabled{ false };
		// Toggles polled off the UI thread, applied by the next UI pass.
		std::atomic<uint32_t> pendingToggles{ 0 };
		std::atomic<Backend> backend{ Backend::kUninitialized };
	};

	[[nodiscard]] PlatformContext& Context() noexcept;

	// Locked functions require it.
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

	void InitializeRendererHealth() noexcept;
	void DrawUiPassLocked() noexcept;
	[[nodiscard]] bool QueryVideoMemoryLocked(uint64_t& a_used, uint64_t& a_budget) noexcept;
	// The pointer in frame-target space, from the native cursor when it is bound.
	[[nodiscard]] PlatformImGui::MousePosition SamplePointerLocked() noexcept;
	[[nodiscard]] bool InstallGameCursorHook() noexcept;

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
