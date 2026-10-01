#include "ImageFileDecode.h"
#include <Support/Runtime.h>

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string_view>

namespace DearModdingUI::PresentationServices::ImageFiles
{
	namespace
	{
		using Microsoft::WRL::ComPtr;

		DMUI_Result ResolvePath(std::string_view a_path, std::filesystem::path& a_resolved)
		{
			if (a_path.empty())
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
				a_path.data(), static_cast<int>(a_path.size()), nullptr, 0);
			if (!count)
				return DMUI_RESULT_INVALID_ARGUMENT;
			std::wstring text(count, L'\0');
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a_path.data(),
				static_cast<int>(a_path.size()), text.data(), count);
			std::replace(text.begin(), text.end(), L'/', L'\\');
			if (text.front() == L'\\')
				return DMUI_RESULT_ACCESS_DENIED;
			const std::filesystem::path path{ text };
			auto normalized = path.lexically_normal();
			if (path.is_absolute())
			{
				const auto root = path.root_path().wstring();
				const auto drive = GetDriveTypeW(root.c_str());
				if (drive == DRIVE_REMOTE || drive == DRIVE_UNKNOWN || drive == DRIVE_NO_ROOT_DIR)
					return DMUI_RESULT_ACCESS_DENIED;
			}
			else
			{
				if (path.has_root_path() || *normalized.begin() == L"..")
					return DMUI_RESULT_ACCESS_DENIED;
				normalized = std::filesystem::path{ Addictol::Support::GetRuntimeDirectory() } /
					L"Data" / normalized;
			}
			// Win32 strips trailing dots/spaces; reject aliases before opening the virtual path.
			for (const auto& component : path.relative_path())
			{
				const auto value = component.wstring();
				if (value == L"." || value == L".." || value.empty())
					continue;
				if (value.back() == L'.' || value.back() == L' ' ||
					value.find_first_of(L":*?\"<>|") != std::wstring::npos)
					return DMUI_RESULT_ACCESS_DENIED;
			}
			a_resolved = std::move(normalized);
			return DMUI_RESULT_OK;
		}

		DMUI_Result ReadFileBytes(const std::filesystem::path& a_path, std::vector<uint8_t>& a_bytes)
		{
			const auto file = CreateFileW(a_path.c_str(), GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				const auto error = GetLastError();
				if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
					return DMUI_RESULT_FILE_NOT_FOUND;
				return error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION ?
					DMUI_RESULT_ACCESS_DENIED : DMUI_RESULT_IMAGE_DECODE_FAILED;
			}
			const std::unique_ptr<void, decltype(&CloseHandle)> closer{ file, CloseHandle };
			if (GetFileType(file) != FILE_TYPE_DISK)
				return DMUI_RESULT_ACCESS_DENIED;
			LARGE_INTEGER size{};
			if (!GetFileSizeEx(file, &size) || size.QuadPart < 0)
				return DMUI_RESULT_IMAGE_DECODE_FAILED;
			if (size.QuadPart > kMaximumBytes)
				return DMUI_RESULT_IMAGE_TOO_LARGE;
			a_bytes.resize(static_cast<size_t>(size.QuadPart));
			DWORD read{};
			if (!ReadFile(file, a_bytes.data(), static_cast<DWORD>(a_bytes.size()), &read, nullptr) ||
				read != a_bytes.size())
				return DMUI_RESULT_IMAGE_DECODE_FAILED;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DecodeWIC(std::vector<uint8_t>& a_bytes, DecodedImage& a_image)
		{
			ComPtr<IWICImagingFactory> factory;
			if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
					IID_PPV_ARGS(&factory))))
				return DMUI_RESULT_IMAGE_DECODE_FAILED;
			ComPtr<IWICStream> stream;
			ComPtr<IWICBitmapDecoder> decoder;
			if (FAILED(factory->CreateStream(&stream)) ||
				FAILED(stream->InitializeFromMemory(a_bytes.data(), static_cast<DWORD>(a_bytes.size()))))
				return DMUI_RESULT_IMAGE_DECODE_FAILED;
			const auto decoded = factory->CreateDecoderFromStream(
				stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
			if (FAILED(decoded))
				return decoded == WINCODEC_ERR_COMPONENTNOTFOUND ?
					DMUI_RESULT_UNSUPPORTED_RESOURCE : DMUI_RESULT_IMAGE_DECODE_FAILED;
			GUID container{};
			if (FAILED(decoder->GetContainerFormat(&container)))
				return DMUI_RESULT_IMAGE_DECODE_FAILED;
			if (container != GUID_ContainerFormatPng && container != GUID_ContainerFormatJpeg &&
				container != GUID_ContainerFormatBmp && container != GUID_ContainerFormatGif &&
				container != GUID_ContainerFormatTiff)
				return DMUI_RESULT_UNSUPPORTED_RESOURCE;
			ComPtr<IWICBitmapFrameDecode> frame;
			if (FAILED(decoder->GetFrame(0, &frame)) ||
				FAILED(frame->GetSize(&a_image.width, &a_image.height)) ||
				!a_image.width || !a_image.height)
				return DMUI_RESULT_IMAGE_DECODE_FAILED;
			const auto size = static_cast<uint64_t>(a_image.width) * a_image.height * 4;
			if (a_image.width > kMaximumDimension || a_image.height > kMaximumDimension ||
				size > kMaximumBytes)
				return DMUI_RESULT_IMAGE_TOO_LARGE;
			ComPtr<IWICFormatConverter> converter;
			if (FAILED(factory->CreateFormatConverter(&converter)) ||
				FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
					WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
				return DMUI_RESULT_IMAGE_DECODE_FAILED;
			a_image.bytes.resize(static_cast<size_t>(size));
			const auto pitch = a_image.width * 4;
			if (FAILED(converter->CopyPixels(nullptr, pitch,
					static_cast<UINT>(size), a_image.bytes.data())))
				return DMUI_RESULT_IMAGE_DECODE_FAILED;
			a_image.mips.push_back({ 0, pitch, static_cast<uint32_t>(size) });
			return DMUI_RESULT_OK;
		}
	}

	DMUI_Result DecodeFile(const std::string& a_path, DecodedImage& a_image) noexcept
	{
		try
		{
			std::filesystem::path path;
			if (const auto result = ResolvePath(a_path, path); result != DMUI_RESULT_OK)
				return result;
			std::vector<uint8_t> bytes;
			if (const auto result = ReadFileBytes(path, bytes); result != DMUI_RESULT_OK)
				return result;
			if (bytes.size() >= 4 && std::memcmp(bytes.data(), "DDS ", 4) == 0)
				return DecodeDDS(std::move(bytes), a_image);
			return DecodeWIC(bytes, a_image);
		}
		catch (const std::bad_alloc&)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
		catch (...)
		{
			return DMUI_RESULT_IMAGE_DECODE_FAILED;
		}
	}
}
