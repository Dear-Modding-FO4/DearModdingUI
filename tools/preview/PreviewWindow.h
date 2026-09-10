#pragma once

#include <Windows.h>

#include <cstdint>
#include <string>

namespace DearModdingUIPreview
{
	class PreviewRenderer;

	class PreviewWindow final
	{
	public:
		explicit PreviewWindow(PreviewRenderer& a_renderer) noexcept;
		~PreviewWindow();

		PreviewWindow(const PreviewWindow&) = delete;
		PreviewWindow(PreviewWindow&&) = delete;
		PreviewWindow& operator=(const PreviewWindow&) = delete;
		PreviewWindow& operator=(PreviewWindow&&) = delete;

		[[nodiscard]] bool Create(
			uint32_t a_width,
			uint32_t a_height,
			bool a_headless,
			std::wstring& a_error);
		void Show() noexcept;
		void SetImguiBackendReady(bool a_ready) noexcept;
		[[nodiscard]] bool PumpMessages() noexcept;
		[[nodiscard]] HWND Handle() const noexcept;

	private:
		static LRESULT CALLBACK WindowProcedure(
			HWND a_window,
			UINT a_message,
			WPARAM a_wparam,
			LPARAM a_lparam);
		[[nodiscard]] LRESULT HandleMessage(
			HWND a_window,
			UINT a_message,
			WPARAM a_wparam,
			LPARAM a_lparam);

		PreviewRenderer& m_renderer;
		HINSTANCE m_instance{};
		HWND m_window{};
		bool m_imguiBackendReady{};
		bool m_escapeConsumed{};
	};
}
