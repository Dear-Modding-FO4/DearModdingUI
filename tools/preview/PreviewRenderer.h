#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace DearModdingUIPreview
{
	class PreviewRenderer final
	{
	public:
		[[nodiscard]] bool Initialize(
			HWND a_window,
			uint32_t a_width,
			uint32_t a_height,
			std::wstring& a_error);
		void RequestResize(uint32_t a_width, uint32_t a_height) noexcept;
		[[nodiscard]] bool ApplyResize(std::wstring& a_error);
		void Clear() noexcept;
		void BindBackBuffer() noexcept;
		[[nodiscard]] bool Present(std::wstring& a_error);
		[[nodiscard]] bool Capture(
			const std::filesystem::path& a_path,
			std::wstring& a_error);

		[[nodiscard]] ID3D11Device* Device() const noexcept;
		[[nodiscard]] ID3D11DeviceContext* Context() const noexcept;
		[[nodiscard]] ID3D11Texture2D* BackBuffer() const noexcept;
		[[nodiscard]] ID3D11RenderTargetView* BackBufferView() const noexcept;
		[[nodiscard]] uint32_t Height() const noexcept;

	private:
		[[nodiscard]] HRESULT CreateDevice(
			D3D_DRIVER_TYPE a_driverType,
			DXGI_SWAP_CHAIN_DESC a_description) noexcept;
		[[nodiscard]] bool CreateBackBuffer(std::wstring& a_error);
		void Reset() noexcept;

		Microsoft::WRL::ComPtr<ID3D11Device> m_device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_backBuffer;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_backBufferView;
		uint32_t m_width{};
		uint32_t m_height{};
		uint32_t m_pendingWidth{};
		uint32_t m_pendingHeight{};
	};
}
