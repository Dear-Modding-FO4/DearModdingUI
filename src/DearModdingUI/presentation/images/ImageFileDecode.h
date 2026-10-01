#pragma once

#include <DearModdingUI/API.h>
#include <d3d11.h>
#include <cstdint>
#include <string>
#include <vector>

namespace DearModdingUI::PresentationServices::ImageFiles
{
	inline constexpr uint32_t kMaximumDimension{ 8192 };
	inline constexpr size_t kMaximumBytes{ 64 * 1024 * 1024 };

	struct Mip
	{
		size_t offset{};
		uint32_t rowPitch{};
		uint32_t slicePitch{};
	};

	struct DecodedImage
	{
		uint32_t width{};
		uint32_t height{};
		DXGI_FORMAT format{ DXGI_FORMAT_R8G8B8A8_UNORM };
		std::vector<uint8_t> bytes;
		std::vector<Mip> mips;
	};

	[[nodiscard]] DMUI_Result DecodeFile(const std::string& a_path, DecodedImage& a_image) noexcept;
	[[nodiscard]] DMUI_Result DecodeDDS(std::vector<uint8_t> a_bytes, DecodedImage& a_image);
}
