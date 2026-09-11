#include "PreviewWindow.h"

#include "PreviewRenderer.h"

#include <DearModdingUI/host/MenuDismissal.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/Host.h>
#include <Platform/input/CursorLoader.h>
#include <Platform/rendering/ImGuiPlatformTargets.h>

#include <imgui/backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND a_window,
	UINT a_message,
	WPARAM a_wparam,
	LPARAM a_lparam);

namespace DearModdingUIPreview
{
	using namespace DearModdingUI;

	namespace
	{
		inline constexpr wchar_t kWindowClassName[]{
			L"DearModdingUIPreviewWindow"
		};
	}

	PreviewWindow::PreviewWindow(PreviewRenderer& a_renderer) noexcept :
		m_renderer(a_renderer)
	{}

	PreviewWindow::~PreviewWindow()
	{
		if (m_window)
			DestroyWindow(m_window);
		if (m_instance)
			UnregisterClassW(kWindowClassName, m_instance);
	}

	bool PreviewWindow::Create(
		uint32_t a_width,
		uint32_t a_height,
		bool a_headless,
		std::wstring& a_error)
	{
		m_instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.style = CS_CLASSDC;
		windowClass.lpfnWndProc = WindowProcedure;
		windowClass.hInstance = m_instance;
		windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
		windowClass.lpszClassName = kWindowClassName;
		if (!RegisterClassExW(&windowClass))
		{
			a_error = L"Could not register the preview window class.";
			return false;
		}

		const auto style = a_headless ?
			static_cast<DWORD>(WS_POPUP) :
			static_cast<DWORD>(WS_OVERLAPPEDWINDOW);
		RECT bounds{
			0,
			0,
			static_cast<LONG>(a_width),
			static_cast<LONG>(a_height)
		};
		if (!a_headless && !AdjustWindowRectEx(&bounds, style, FALSE, 0))
		{
			a_error = L"Could not size the preview window.";
			return false;
		}
		m_window = CreateWindowExW(
			0,
			kWindowClassName,
			L"DearModdingUI Preview",
			style,
			a_headless ? 0 : CW_USEDEFAULT,
			a_headless ? 0 : CW_USEDEFAULT,
			bounds.right - bounds.left,
			bounds.bottom - bounds.top,
			nullptr,
			nullptr,
			m_instance,
			this);
		if (!m_window)
		{
			a_error = L"Could not create the preview window.";
			return false;
		}
		return true;
	}

	void PreviewWindow::Show() noexcept
	{
		ShowWindow(m_window, SW_SHOWDEFAULT);
		UpdateWindow(m_window);
	}

	void PreviewWindow::SetImguiBackendReady(bool a_ready) noexcept
	{
		m_imguiBackendReady = a_ready;
	}

	bool PreviewWindow::PumpMessages() noexcept
	{
		MSG message{};
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
				return false;
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		return true;
	}

	HWND PreviewWindow::Handle() const noexcept
	{
		return m_window;
	}

	LRESULT CALLBACK PreviewWindow::WindowProcedure(
		HWND a_window,
		UINT a_message,
		WPARAM a_wparam,
		LPARAM a_lparam)
	{
		PreviewWindow* self{};
		if (a_message == WM_NCCREATE)
		{
			const auto* creation =
				reinterpret_cast<const CREATESTRUCTW*>(a_lparam);
			self = static_cast<PreviewWindow*>(creation->lpCreateParams);
			SetWindowLongPtrW(
				a_window,
				GWLP_USERDATA,
				reinterpret_cast<LONG_PTR>(self));
		}
		else
		{
			self = reinterpret_cast<PreviewWindow*>(
				GetWindowLongPtrW(a_window, GWLP_USERDATA));
		}
		return self ?
			self->HandleMessage(
				a_window,
				a_message,
				a_wparam,
				a_lparam) :
			DefWindowProcW(a_window, a_message, a_wparam, a_lparam);
	}

	LRESULT PreviewWindow::HandleMessage(
		HWND a_window,
		UINT a_message,
		WPARAM a_wparam,
		LPARAM a_lparam)
	{
		if (CursorLoader::HandleWindowMessage(
				a_window, a_message, static_cast<uint64_t>(a_lparam)))
			return 1;
		const auto escapeDecision =
			Addictol::ImguiPlatform::DecideEscapeMessage(
				a_message,
				static_cast<uint32_t>(a_wparam),
				static_cast<uint64_t>(a_lparam),
				IsMenuVisible(),
				m_escapeConsumed);
		if (escapeDecision ==
			Addictol::ImguiPlatform::EscapeMessageDecision::kCapture)
		{
			m_escapeConsumed = true;
			CaptureMenuEscapePress(
				IsMenuVisible(),
				PresentationServices::HasActiveDialog(),
				PresentationServices::ActiveDialogPopupId());
		}
		else if (escapeDecision ==
				Addictol::ImguiPlatform::EscapeMessageDecision::
					kConsumeAndRelease ||
			escapeDecision ==
				Addictol::ImguiPlatform::EscapeMessageDecision::
					kReleaseAndForward)
		{
			m_escapeConsumed = false;
		}

		const auto imguiHandled =
			m_imguiBackendReady &&
			ImGui_ImplWin32_WndProcHandler(
				a_window,
				a_message,
				a_wparam,
				a_lparam);
		if (escapeDecision !=
				Addictol::ImguiPlatform::EscapeMessageDecision::kForward &&
			escapeDecision !=
				Addictol::ImguiPlatform::EscapeMessageDecision::
					kReleaseAndForward)
			return 1;
		if (imguiHandled)
			return 1;

		switch (a_message)
		{
		case WM_SIZE:
			if (a_wparam != SIZE_MINIMIZED)
			{
				m_renderer.RequestResize(
					static_cast<uint32_t>(LOWORD(a_lparam)),
					static_cast<uint32_t>(HIWORD(a_lparam)));
			}
			return 0;
		case WM_SYSCOMMAND:
			if ((a_wparam & 0xFFF0u) == SC_KEYMENU)
				return 0;
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			break;
		}
		return DefWindowProcW(a_window, a_message, a_wparam, a_lparam);
	}
}
