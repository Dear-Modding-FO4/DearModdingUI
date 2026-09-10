#pragma once

#include "../Harness.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace vmm_tests::support
{
	struct D3DTestResources
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
	};

	[[nodiscard]] inline D3DTestResources CreateImageResources()
	{
		D3DTestResources resources;
		D3D_FEATURE_LEVEL level{};
		require(SUCCEEDED(D3D11CreateDevice(
					nullptr,
					D3D_DRIVER_TYPE_WARP,
					nullptr,
					0,
					nullptr,
					0,
					D3D11_SDK_VERSION,
					&resources.device,
					&level,
					&resources.context)),
			"WARP D3D11 device creation failed");
		const D3D11_TEXTURE2D_DESC textureDescription{
			64,
			32,
			1,
			1,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			{ 1, 0 },
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_SHADER_RESOURCE,
			0,
			0
		};
		require(SUCCEEDED(resources.device->CreateTexture2D(
					&textureDescription,
					nullptr,
					&resources.texture)),
			"test texture creation failed");
		require(SUCCEEDED(resources.device->CreateShaderResourceView(
					resources.texture.Get(),
					nullptr,
					&resources.view)),
			"test SRV creation failed");
		return resources;
	}

	[[nodiscard]] inline ULONG ReferenceCount(IUnknown* a_object) noexcept
	{
		const auto incremented = a_object->AddRef();
		(void)a_object->Release();
		return incremented - 1;
	}

	[[nodiscard]] inline std::vector<uint8_t> ReadPixels(
		ID3D11Device* a_device,
		ID3D11DeviceContext* a_context,
		ID3D11ShaderResourceView* a_view,
		uint32_t& a_width,
		uint32_t& a_height)
	{
		require(a_view != nullptr, "queued image view was null");
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		a_view->GetResource(&resource);
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		require(SUCCEEDED(resource.As(&texture)),
			"queued image resource was not Texture2D");
		D3D11_TEXTURE2D_DESC description{};
		texture->GetDesc(&description);
		require(description.Format == DXGI_FORMAT_R8G8B8A8_UNORM,
			"CPU image did not use RGBA8 UNORM");
		a_width = description.Width;
		a_height = description.Height;
		description.Usage = D3D11_USAGE_STAGING;
		description.BindFlags = 0;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		description.MiscFlags = 0;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
		require(SUCCEEDED(a_device->CreateTexture2D(
					&description,
					nullptr,
					&staging)),
			"staging texture creation failed");
		a_context->CopyResource(staging.Get(), texture.Get());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		require(SUCCEEDED(a_context->Map(
					staging.Get(),
					0,
					D3D11_MAP_READ,
					0,
					&mapped)),
			"staging texture map failed");
		std::vector<uint8_t> result(
			static_cast<size_t>(a_width) * a_height * 4u);
		for (uint32_t row = 0; row < a_height; ++row)
		{
			std::memcpy(
				result.data() + static_cast<size_t>(row) * a_width * 4u,
				static_cast<const uint8_t*>(mapped.pData) +
					static_cast<size_t>(row) * mapped.RowPitch,
				static_cast<size_t>(a_width) * 4u);
		}
		a_context->Unmap(staging.Get(), 0);
		return result;
	}
}
