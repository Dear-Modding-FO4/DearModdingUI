#include "ImageFileDecode.h"

// Keep the upstream parser private while separating CPU validation from GPU creation.
#include <directx-dds/DDSTextureLoader11.cpp>

#include <bit>

namespace DearModdingUI::PresentationServices::ImageFiles
{
	DMUI_Result DecodeDDS(std::vector<uint8_t> a_bytes, DecodedImage& a_image)
	{
		const DDS_HEADER* header{};
		const uint8_t* bits{};
		size_t bitSize{};
		if (FAILED(LoadTextureDataFromMemory(
				a_bytes.data(), a_bytes.size(), &header, &bits, &bitSize)))
			return DMUI_RESULT_IMAGE_DECODE_FAILED;
		if ((header->caps2 & DDS_CUBEMAP) ||
			(header->flags & DDS_HEADER_FLAGS_VOLUME) || header->depth > 1)
			return DMUI_RESULT_UNSUPPORTED_RESOURCE;

		auto format = GetDXGIFormat(header->ddspf);
		if ((header->ddspf.flags & DDS_FOURCC) &&
			header->ddspf.fourCC == MAKEFOURCC('D', 'X', '1', '0'))
		{
			const auto* extension = reinterpret_cast<const DDS_HEADER_DXT10*>(header + 1);
			if (extension->resourceDimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D ||
				extension->arraySize != 1 ||
				(extension->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE))
				return DMUI_RESULT_UNSUPPORTED_RESOURCE;
			if ((extension->miscFlags2 & 7u) == DDS_ALPHA_MODE_PREMULTIPLIED)
				return DMUI_RESULT_UNSUPPORTED_RESOURCE;
			format = extension->dxgiFormat;
		}
		if (header->ddspf.fourCC == MAKEFOURCC('D', 'X', 'T', '2') ||
			header->ddspf.fourCC == MAKEFOURCC('D', 'X', 'T', '4'))
			return DMUI_RESULT_UNSUPPORTED_RESOURCE;
		format = MakeLinear(format);
		switch (format)
		{
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
		case DXGI_FORMAT_B4G4R4A4_UNORM:
			break;
		default:
			return DMUI_RESULT_UNSUPPORTED_RESOURCE;
		}
		if (!header->width || !header->height)
			return DMUI_RESULT_IMAGE_DECODE_FAILED;
		if (header->width > kMaximumDimension || header->height > kMaximumDimension)
			return DMUI_RESULT_IMAGE_TOO_LARGE;
		const auto mipCount = (std::max)(header->mipMapCount, 1u);
		if (mipCount > static_cast<uint32_t>(std::bit_width((std::max)(header->width, header->height))))
			return DMUI_RESULT_IMAGE_DECODE_FAILED;
		std::vector<D3D11_SUBRESOURCE_DATA> initial(mipCount);
		size_t width{}, height{}, depth{}, skip{};
		if (FAILED(FillInitData(header->width, header->height, 1, mipCount, 1,
				format, 0, bitSize, bits, width, height, depth, skip, initial.data())))
			return DMUI_RESULT_IMAGE_DECODE_FAILED;
		a_image.width = static_cast<uint32_t>(width);
		a_image.height = static_cast<uint32_t>(height);
		a_image.format = format;
		for (const auto& mip : initial)
			a_image.mips.push_back({
				static_cast<size_t>(static_cast<const uint8_t*>(mip.pSysMem) - a_bytes.data()),
				mip.SysMemPitch, mip.SysMemSlicePitch });
		a_image.bytes = std::move(a_bytes);
		return DMUI_RESULT_OK;
	}
}
