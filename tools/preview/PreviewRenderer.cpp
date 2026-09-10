#include "PreviewRenderer.h"

#include <DearModdingUI/presentation/BackgroundBlur.h>

#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>

#include <limits>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace DearModdingUIPreview
{
	using Microsoft::WRL::ComPtr;

	namespace
	{
		void SetHRESULTError(
			std::wstring& a_error,
			std::wstring_view a_operation,
			HRESULT a_result)
		{
			std::wostringstream output;
			output << a_operation << L" failed (0x" << std::hex
				   << static_cast<uint32_t>(a_result) << L").";
			a_error = output.str();
		}
	}

	bool PreviewRenderer::Initialize(
		HWND a_window,
		uint32_t a_width,
		uint32_t a_height,
		std::wstring& a_error)
	{
		DXGI_SWAP_CHAIN_DESC swapChainDescription{};
		swapChainDescription.BufferCount = 2;
		swapChainDescription.BufferDesc.Width = a_width;
		swapChainDescription.BufferDesc.Height = a_height;
		swapChainDescription.BufferDesc.Format =
			DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDescription.OutputWindow = a_window;
		swapChainDescription.SampleDesc.Count = 1;
		swapChainDescription.Windowed = TRUE;
		swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		auto result = CreateDevice(
			D3D_DRIVER_TYPE_HARDWARE,
			swapChainDescription);
		if (FAILED(result))
		{
			Reset();
			result = CreateDevice(
				D3D_DRIVER_TYPE_WARP,
				swapChainDescription);
		}
		if (FAILED(result))
		{
			SetHRESULTError(
				a_error,
				L"D3D11CreateDeviceAndSwapChain",
				result);
			return false;
		}
		return CreateBackBuffer(a_error);
	}

	void PreviewRenderer::RequestResize(
		uint32_t a_width,
		uint32_t a_height) noexcept
	{
		m_pendingWidth = a_width;
		m_pendingHeight = a_height;
	}

	bool PreviewRenderer::ApplyResize(std::wstring& a_error)
	{
		if (!m_pendingWidth || !m_pendingHeight)
			return true;
		const auto width = std::exchange(m_pendingWidth, 0u);
		const auto height = std::exchange(m_pendingHeight, 0u);
		if (width == m_width && height == m_height)
			return true;

		m_context->OMSetRenderTargets(0, nullptr, nullptr);
		DearModdingUI::BackgroundBlur::InvalidateBackBuffer();
		m_backBufferView.Reset();
		m_backBuffer.Reset();
		const auto result = m_swapChain->ResizeBuffers(
			0,
			width,
			height,
			DXGI_FORMAT_UNKNOWN,
			0);
		if (FAILED(result))
		{
			SetHRESULTError(a_error, L"IDXGISwapChain::ResizeBuffers", result);
			return false;
		}
		return CreateBackBuffer(a_error);
	}

	void PreviewRenderer::Clear() noexcept
	{
		constexpr float color[]{ 0.025f, 0.031f, 0.043f, 1.0f };
		m_context->ClearRenderTargetView(m_backBufferView.Get(), color);
	}

	void PreviewRenderer::BindBackBuffer() noexcept
	{
		auto* const view = m_backBufferView.Get();
		m_context->OMSetRenderTargets(1, &view, nullptr);
	}

	bool PreviewRenderer::Present(std::wstring& a_error)
	{
		const auto result = m_swapChain->Present(1, 0);
		if (FAILED(result))
		{
			SetHRESULTError(a_error, L"IDXGISwapChain::Present", result);
			return false;
		}
		return true;
	}

	bool PreviewRenderer::Capture(
		const std::filesystem::path& a_path,
		std::wstring& a_error)
	{
		std::error_code filesystemError;
		if (!a_path.parent_path().empty())
		{
			std::filesystem::create_directories(
				a_path.parent_path(),
				filesystemError);
			if (filesystemError)
			{
				a_error = L"Could not create the screenshot directory.";
				return false;
			}
		}

		D3D11_TEXTURE2D_DESC description{};
		m_backBuffer->GetDesc(&description);
		D3D11_TEXTURE2D_DESC stagingDescription = description;
		stagingDescription.BindFlags = 0;
		stagingDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDescription.MiscFlags = 0;
		stagingDescription.Usage = D3D11_USAGE_STAGING;

		ComPtr<ID3D11Texture2D> staging;
		auto result = m_device->CreateTexture2D(
			&stagingDescription,
			nullptr,
			staging.GetAddressOf());
		if (FAILED(result))
		{
			SetHRESULTError(a_error, L"ID3D11Device::CreateTexture2D", result);
			return false;
		}
		m_context->CopyResource(staging.Get(), m_backBuffer.Get());

		ComPtr<IWICImagingFactory> factory;
		result = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(factory.GetAddressOf()));
		if (FAILED(result))
		{
			SetHRESULTError(a_error, L"CoCreateInstance(WIC)", result);
			return false;
		}

		ComPtr<IWICStream> stream;
		result = factory->CreateStream(stream.GetAddressOf());
		if (SUCCEEDED(result))
			result = stream->InitializeFromFilename(a_path.c_str(), GENERIC_WRITE);
		if (FAILED(result))
		{
			SetHRESULTError(
				a_error,
				L"IWICStream::InitializeFromFilename",
				result);
			return false;
		}

		ComPtr<IWICBitmapEncoder> encoder;
		result = factory->CreateEncoder(
			GUID_ContainerFormatPng,
			nullptr,
			encoder.GetAddressOf());
		if (SUCCEEDED(result))
			result = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
		if (FAILED(result))
		{
			SetHRESULTError(a_error, L"IWICBitmapEncoder::Initialize", result);
			return false;
		}

		ComPtr<IWICBitmapFrameEncode> frame;
		ComPtr<IPropertyBag2> properties;
		result = encoder->CreateNewFrame(
			frame.GetAddressOf(),
			properties.GetAddressOf());
		if (SUCCEEDED(result))
			result = frame->Initialize(properties.Get());
		if (SUCCEEDED(result))
			result = frame->SetSize(description.Width, description.Height);
		WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppRGBA;
		if (SUCCEEDED(result))
			result = frame->SetPixelFormat(&pixelFormat);
		if (FAILED(result))
		{
			SetHRESULTError(
				a_error,
				L"IWICBitmapFrameEncode::Initialize",
				result);
			return false;
		}
		const auto directRgba =
			InlineIsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppRGBA);
		const auto convertToBgra =
			InlineIsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppBGRA);
		if (!directRgba && !convertToBgra)
		{
			a_error = L"WIC does not support a 32-bit PNG pixel format.";
			return false;
		}
		const auto outputStride = description.Width * 4u;
		const auto outputSize =
			static_cast<uint64_t>(outputStride) * description.Height;
		if (convertToBgra &&
			outputSize > (std::numeric_limits<UINT>::max)())
		{
			a_error = L"Converted screenshot buffer is too large for WIC.";
			return false;
		}
		std::vector<BYTE> pixels;
		if (convertToBgra)
			pixels.resize(static_cast<size_t>(outputSize));

		D3D11_MAPPED_SUBRESOURCE mapped{};
		result = m_context->Map(
			staging.Get(),
			0,
			D3D11_MAP_READ,
			0,
			&mapped);
		if (FAILED(result))
		{
			SetHRESULTError(a_error, L"ID3D11DeviceContext::Map", result);
			return false;
		}

		const auto bufferSize =
			static_cast<uint64_t>(mapped.RowPitch) * description.Height;
		if (bufferSize > (std::numeric_limits<UINT>::max)())
		{
			m_context->Unmap(staging.Get(), 0);
			a_error = L"Screenshot buffer is too large for WIC.";
			return false;
		}
		if (directRgba)
		{
			result = frame->WritePixels(
				description.Height,
				mapped.RowPitch,
				static_cast<UINT>(bufferSize),
				static_cast<BYTE*>(mapped.pData));
		}
		else
		{
			for (uint32_t y = 0; y < description.Height; ++y)
			{
				const auto* source =
					static_cast<const BYTE*>(mapped.pData) +
					static_cast<size_t>(mapped.RowPitch) * y;
				auto* destination =
					pixels.data() + static_cast<size_t>(outputStride) * y;
				for (uint32_t x = 0; x < description.Width; ++x)
				{
					destination[x * 4u] = source[x * 4u + 2u];
					destination[x * 4u + 1u] = source[x * 4u + 1u];
					destination[x * 4u + 2u] = source[x * 4u];
					destination[x * 4u + 3u] = source[x * 4u + 3u];
				}
			}
			result = frame->WritePixels(
				description.Height,
				outputStride,
				static_cast<UINT>(outputSize),
				pixels.data());
		}
		m_context->Unmap(staging.Get(), 0);
		if (SUCCEEDED(result))
			result = frame->Commit();
		if (SUCCEEDED(result))
			result = encoder->Commit();
		if (FAILED(result))
		{
			SetHRESULTError(a_error, L"IWICBitmapEncoder::Commit", result);
			return false;
		}
		return true;
	}

	ID3D11Device* PreviewRenderer::Device() const noexcept
	{
		return m_device.Get();
	}

	ID3D11DeviceContext* PreviewRenderer::Context() const noexcept
	{
		return m_context.Get();
	}

	ID3D11Texture2D* PreviewRenderer::BackBuffer() const noexcept
	{
		return m_backBuffer.Get();
	}

	ID3D11RenderTargetView* PreviewRenderer::BackBufferView() const noexcept
	{
		return m_backBufferView.Get();
	}

	uint32_t PreviewRenderer::Height() const noexcept
	{
		return m_height;
	}

	HRESULT PreviewRenderer::CreateDevice(
		D3D_DRIVER_TYPE a_driverType,
		DXGI_SWAP_CHAIN_DESC a_description) noexcept
	{
		return D3D11CreateDeviceAndSwapChain(
			nullptr,
			a_driverType,
			nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT,
			nullptr,
			0,
			D3D11_SDK_VERSION,
			&a_description,
			m_swapChain.GetAddressOf(),
			m_device.GetAddressOf(),
			nullptr,
			m_context.GetAddressOf());
	}

	bool PreviewRenderer::CreateBackBuffer(std::wstring& a_error)
	{
		auto result = m_swapChain->GetBuffer(
			0,
			IID_PPV_ARGS(m_backBuffer.GetAddressOf()));
		if (SUCCEEDED(result))
			result = m_device->CreateRenderTargetView(
				m_backBuffer.Get(),
				nullptr,
				m_backBufferView.GetAddressOf());
		if (FAILED(result))
		{
			SetHRESULTError(a_error, L"Create render target", result);
			return false;
		}
		D3D11_TEXTURE2D_DESC description{};
		m_backBuffer->GetDesc(&description);
		m_width = description.Width;
		m_height = description.Height;
		return true;
	}

	void PreviewRenderer::Reset() noexcept
	{
		m_backBufferView.Reset();
		m_backBuffer.Reset();
		m_swapChain.Reset();
		m_context.Reset();
		m_device.Reset();
	}
}
