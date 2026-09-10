#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <Support/ProcessLifetime.h>
#include "PresentationServiceOwners.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

namespace DearModdingUI::PresentationServices
{
	namespace
	{
		inline constexpr uint32_t kNoImageSlot{
			(std::numeric_limits<uint32_t>::max)()
		};

		enum class ImageProvenance : uint8_t
		{
			kNone,
			kImportedD3D11,
			kCpuPixels
		};

		struct ImageEntry
		{
			DMUI_ClientHandle owner{ DMUI_INVALID_CLIENT_HANDLE };
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
			uint32_t width{};
			uint32_t height{};
			uint64_t deviceGeneration{};
			uint32_t handleGeneration{ 1 };
			uint32_t nextFree{ kNoImageSlot };
			DMUI_ImageStatus status{ DMUI_IMAGE_STATUS_READY };
			ImageProvenance provenance{ ImageProvenance::kNone };
			bool reusable{};
		};

		struct ImageService
		{
			std::mutex mutex;
			ID3D11Device* device{};
			uint64_t deviceGeneration{ 1 };
			std::vector<ImageEntry> images;
			uint32_t firstFreeImage{ kNoImageSlot };
			std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
				frameLeases;
		};

		[[nodiscard]] ImageService& GetImageService() noexcept
		{
			static Addictol::Support::ProcessLifetime<ImageService> service;
			return service.value;
		}

		[[nodiscard]] bool SupportedFormat(DXGI_FORMAT a_format) noexcept
		{
			switch (a_format)
			{
			case DXGI_FORMAT_R8_UNORM:
			case DXGI_FORMAT_R8G8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			case DXGI_FORMAT_R11G11B10_FLOAT:
			case DXGI_FORMAT_R16_UNORM:
			case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_R16G16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_R32G32_FLOAT:
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
				return true;
			default:
				return false;
			}
		}

		[[nodiscard]] ImageEntry* FindImage(
			ImageService& a_service,
			DMUI_ImageHandle a_image) noexcept
		{
			const auto slotToken = static_cast<uint32_t>(a_image);
			const auto handleGeneration =
				static_cast<uint32_t>(a_image >> 32u);
			if (!slotToken || !handleGeneration)
				return nullptr;
			const auto slot = slotToken - 1u;
			if (slot >= a_service.images.size())
				return nullptr;
			auto& image = a_service.images[slot];
			return image.handleGeneration == handleGeneration ?
				&image :
				nullptr;
		}

		[[nodiscard]] DMUI_ImageHandle MakeImageHandle(
			uint32_t a_slot,
			uint32_t a_generation) noexcept
		{
			return (static_cast<uint64_t>(a_generation) << 32u) |
				(static_cast<uint64_t>(a_slot) + 1u);
		}

		void RecycleImage(
			ImageService& a_service,
			uint32_t a_slot,
			DMUI_ImageStatus a_status) noexcept
		{
			auto& image = a_service.images[a_slot];
			image.view.Reset();
			image.status = a_status;
			image.provenance = ImageProvenance::kNone;
			if (!image.reusable &&
				image.handleGeneration !=
					(std::numeric_limits<uint32_t>::max)())
			{
				image.nextFree = a_service.firstFreeImage;
				image.reusable = true;
				a_service.firstFreeImage = a_slot;
			}
		}

		[[nodiscard]] DMUI_Result PublishImage(
			ImageService& a_service,
			DMUI_ClientHandle a_client,
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> a_view,
			uint32_t a_width,
			uint32_t a_height,
			ImageProvenance a_provenance,
			DMUI_ImageHandle* a_image) noexcept
		{
			uint32_t slot{ kNoImageSlot };
			while (a_service.firstFreeImage != kNoImageSlot)
			{
				slot = a_service.firstFreeImage;
				auto& candidate = a_service.images[slot];
				a_service.firstFreeImage = candidate.nextFree;
				candidate.nextFree = kNoImageSlot;
				candidate.reusable = false;
				if (candidate.handleGeneration !=
					(std::numeric_limits<uint32_t>::max)())
				{
					++candidate.handleGeneration;
					break;
				}
				slot = kNoImageSlot;
			}

			if (slot == kNoImageSlot)
			{
				if (a_service.images.size() >=
					(std::numeric_limits<uint32_t>::max)())
					return DMUI_RESULT_RESOURCE_EXHAUSTED;
				try
				{
					a_service.images.push_back({
						a_client,
						std::move(a_view),
						a_width,
						a_height,
						a_service.deviceGeneration,
						1,
						kNoImageSlot,
						DMUI_IMAGE_STATUS_READY,
						a_provenance
					});
					slot = static_cast<uint32_t>(
						a_service.images.size() - 1u);
				}
				catch (const std::bad_alloc&)
				{
					return DMUI_RESULT_RESOURCE_EXHAUSTED;
				}
			}
			else
			{
				auto& image = a_service.images[slot];
				image.owner = a_client;
				image.view = std::move(a_view);
				image.width = a_width;
				image.height = a_height;
				image.deviceGeneration = a_service.deviceGeneration;
				image.status = DMUI_IMAGE_STATUS_READY;
				image.provenance = a_provenance;
			}
			*a_image = MakeImageHandle(
				slot, a_service.images[slot].handleGeneration);
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] DMUI_Result ValidatePixelDescriptor(
			const DMUI_ImageDescriptor& a_descriptor,
			uint64_t& a_tightRowPitch,
			uint64_t& a_requiredBytes) noexcept
		{
			if (a_descriptor.pixelFormat != DMUI_PIXEL_FORMAT_RGBA8_UNORM)
				return DMUI_RESULT_UNSUPPORTED_RESOURCE;
			if (a_descriptor.reserved != 0)
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			if (!a_descriptor.pixels ||
				a_descriptor.width == 0 ||
				a_descriptor.height == 0)
				return DMUI_RESULT_INVALID_ARGUMENT;

			a_tightRowPitch =
				static_cast<uint64_t>(a_descriptor.width) * UINT64_C(4);
			if (a_descriptor.rowPitch < a_tightRowPitch ||
				a_descriptor.rowPitch >
					(std::numeric_limits<uint32_t>::max)())
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto precedingRows =
				static_cast<uint64_t>(a_descriptor.height - 1u);
			if (precedingRows >
				((std::numeric_limits<uint64_t>::max)() -
					a_tightRowPitch) /
					a_descriptor.rowPitch)
				return DMUI_RESULT_INVALID_ARGUMENT;
			a_requiredBytes =
				precedingRows * a_descriptor.rowPitch + a_tightRowPitch;
			if (a_descriptor.accessibleByteCount < a_requiredBytes)
				return DMUI_RESULT_INVALID_ARGUMENT;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] uint32_t MaximumTextureDimension(
			D3D_FEATURE_LEVEL a_level) noexcept
		{
			if (a_level >= D3D_FEATURE_LEVEL_11_0)
				return D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
			if (a_level >= D3D_FEATURE_LEVEL_10_0)
				return D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
			if (a_level >= D3D_FEATURE_LEVEL_9_3)
				return D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION;
			return D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION;
		}

		[[nodiscard]] DMUI_Result CreatePixelView(
			ID3D11Device* a_device,
			const DMUI_ImageDescriptor& a_descriptor,
			uint64_t a_tightRowPitch,
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& a_view) noexcept
		{
			const auto maximumDimension =
				MaximumTextureDimension(a_device->GetFeatureLevel());
			if (a_descriptor.width > maximumDimension ||
				a_descriptor.height > maximumDimension)
				return DMUI_RESULT_INVALID_ARGUMENT;

			std::vector<uint8_t> pixels;
			const auto byteCount =
				a_tightRowPitch * static_cast<uint64_t>(a_descriptor.height);
			try
			{
				pixels.resize(static_cast<size_t>(byteCount));
			}
			catch (const std::bad_alloc&)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
			const auto* source = static_cast<const uint8_t*>(
				a_descriptor.pixels);
			for (uint32_t row = 0; row < a_descriptor.height; ++row)
			{
				std::memcpy(
					pixels.data() + static_cast<size_t>(row * a_tightRowPitch),
					source + static_cast<size_t>(
						static_cast<uint64_t>(row) * a_descriptor.rowPitch),
					static_cast<size_t>(a_tightRowPitch));
			}

			const D3D11_TEXTURE2D_DESC textureDescription{
				a_descriptor.width,
				a_descriptor.height,
				1,
				1,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				{ 1, 0 },
				D3D11_USAGE_IMMUTABLE,
				D3D11_BIND_SHADER_RESOURCE,
				0,
				0
			};
			const D3D11_SUBRESOURCE_DATA initialData{
				pixels.data(),
				static_cast<uint32_t>(a_tightRowPitch),
				0
			};
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
			if (FAILED(a_device->CreateTexture2D(
					&textureDescription,
					&initialData,
					&texture)))
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			if (FAILED(a_device->CreateShaderResourceView(
					texture.Get(),
					nullptr,
					&a_view)))
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			return DMUI_RESULT_OK;
		}
	}

	DMUI_Result ImportD3D11Image(
		DMUI_ClientHandle a_client,
		const DMUI_D3D11ImageDescriptor* a_descriptor,
		DMUI_ImageHandle* a_image) noexcept
	{
		if (!a_descriptor || !a_image ||
			a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_image = DMUI_INVALID_IMAGE_HANDLE;
		if (a_descriptor->structSize < DMUI_D3D11_IMAGE_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!a_descriptor->shaderResourceView)
			return DMUI_RESULT_INVALID_ARGUMENT;
		{
			auto& service = GetImageService();
			const std::scoped_lock lock{ service.mutex };
			if (!RenderExecution::IsActive())
				return DMUI_RESULT_WRONG_THREAD;
		}

		auto* view = static_cast<ID3D11ShaderResourceView*>(
			a_descriptor->shaderResourceView);
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
		view->GetDesc(&viewDescription);
		if (viewDescription.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D ||
			!SupportedFormat(viewDescription.Format))
			return DMUI_RESULT_UNSUPPORTED_RESOURCE;

		ID3D11Resource* resource{};
		view->GetResource(&resource);
		if (!resource)
			return DMUI_RESULT_UNSUPPORTED_RESOURCE;
		ID3D11Texture2D* texture{};
		const auto queryResult = resource->QueryInterface(
			__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&texture));
		resource->Release();
		if (FAILED(queryResult) || !texture)
			return DMUI_RESULT_UNSUPPORTED_RESOURCE;
		D3D11_TEXTURE2D_DESC textureDescription{};
		texture->GetDesc(&textureDescription);
		texture->Release();
		if (textureDescription.ArraySize != 1 ||
			textureDescription.SampleDesc.Count != 1 ||
			viewDescription.Texture2D.MostDetailedMip >= textureDescription.MipLevels)
			return DMUI_RESULT_UNSUPPORTED_RESOURCE;

		ID3D11Device* imageDevice{};
		view->GetDevice(&imageDevice);
		constexpr UINT requiredSupport =
			D3D11_FORMAT_SUPPORT_TEXTURE2D |
			D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;
		UINT formatSupport{};
		const auto sampleable =
			imageDevice &&
			SUCCEEDED(imageDevice->CheckFormatSupport(
				viewDescription.Format, &formatSupport)) &&
			(formatSupport & requiredSupport) == requiredSupport;
		auto& service = GetImageService();
		const std::scoped_lock lock{ service.mutex };
		const auto deviceMatches = imageDevice && imageDevice == service.device;
		if (imageDevice)
			imageDevice->Release();
		if (!deviceMatches || !service.device || !sampleable)
			return DMUI_RESULT_UNSUPPORTED_RESOURCE;
		const auto mip = viewDescription.Texture2D.MostDetailedMip;
		const auto derivedWidth = (std::max)(textureDescription.Width >> mip, 1u);
		const auto derivedHeight = (std::max)(textureDescription.Height >> mip, 1u);
		const auto width = a_descriptor->contentWidth ?
			a_descriptor->contentWidth :
			derivedWidth;
		const auto height = a_descriptor->contentHeight ?
			a_descriptor->contentHeight :
			derivedHeight;
		if (width > derivedWidth || height > derivedHeight)
			return DMUI_RESULT_INVALID_ARGUMENT;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> retainedView{ view };
		return PublishImage(
			service,
			a_client,
			std::move(retainedView),
			width,
			height,
			ImageProvenance::kImportedD3D11,
			a_image);
	}

	DMUI_Result CreateImage(
		DMUI_ClientHandle a_client,
		const DMUI_ImageDescriptor* a_descriptor,
		DMUI_ImageHandle* a_image) noexcept
	{
		if (!a_descriptor || !a_image ||
			a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_image = DMUI_INVALID_IMAGE_HANDLE;
		if (a_descriptor->structSize < DMUI_IMAGE_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		uint64_t tightRowPitch{};
		uint64_t requiredBytes{};
		const auto descriptorResult = ValidatePixelDescriptor(
			*a_descriptor, tightRowPitch, requiredBytes);
		if (descriptorResult != DMUI_RESULT_OK)
			return descriptorResult;
		(void)requiredBytes;

		Microsoft::WRL::ComPtr<ID3D11Device> device;
		uint64_t deviceGeneration{};
		{
			auto& service = GetImageService();
			const std::scoped_lock lock{ service.mutex };
			if (!RenderExecution::IsActive())
				return DMUI_RESULT_WRONG_THREAD;
			if (!service.device)
				return DMUI_RESULT_HOST_NOT_READY;
			device = service.device;
			deviceGeneration = service.deviceGeneration;
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
		const auto createResult = CreatePixelView(
			device.Get(), *a_descriptor, tightRowPitch, view);
		if (createResult != DMUI_RESULT_OK)
			return createResult;

		auto& service = GetImageService();
		const std::scoped_lock lock{ service.mutex };
		if (!RenderExecution::IsActive())
			return DMUI_RESULT_WRONG_THREAD;
		if (service.device != device.Get() ||
			service.deviceGeneration != deviceGeneration)
			return DMUI_RESULT_HOST_NOT_READY;
		return PublishImage(
			service,
			a_client,
			std::move(view),
			a_descriptor->width,
			a_descriptor->height,
			ImageProvenance::kCpuPixels,
			a_image);
	}

	DMUI_Result UpdateImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image,
		const DMUI_ImageDescriptor* a_descriptor) noexcept
	{
		if (!a_descriptor ||
			a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_image == DMUI_INVALID_IMAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_descriptor->structSize < DMUI_IMAGE_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		uint64_t tightRowPitch{};
		uint64_t requiredBytes{};
		const auto descriptorResult = ValidatePixelDescriptor(
			*a_descriptor, tightRowPitch, requiredBytes);
		if (descriptorResult != DMUI_RESULT_OK)
			return descriptorResult;
		(void)requiredBytes;

		Microsoft::WRL::ComPtr<ID3D11Device> device;
		uint64_t deviceGeneration{};
		{
			auto& service = GetImageService();
			const std::scoped_lock lock{ service.mutex };
			if (!RenderExecution::IsActive())
				return DMUI_RESULT_WRONG_THREAD;
			auto* image = FindImage(service, a_image);
			if (!image || image->owner != a_client ||
				image->status != DMUI_IMAGE_STATUS_READY ||
				image->deviceGeneration != service.deviceGeneration ||
				!image->view)
				return DMUI_RESULT_STALE_HANDLE;
			if (image->provenance != ImageProvenance::kCpuPixels)
				return DMUI_RESULT_UNSUPPORTED_RESOURCE;
			if (!service.device)
				return DMUI_RESULT_HOST_NOT_READY;
			device = service.device;
			deviceGeneration = service.deviceGeneration;
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> replacement;
		const auto createResult = CreatePixelView(
			device.Get(), *a_descriptor, tightRowPitch, replacement);
		if (createResult != DMUI_RESULT_OK)
			return createResult;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> previous;
		{
			auto& service = GetImageService();
			const std::scoped_lock lock{ service.mutex };
			if (!RenderExecution::IsActive())
				return DMUI_RESULT_WRONG_THREAD;
			auto* image = FindImage(service, a_image);
			if (!image || image->owner != a_client ||
				image->status != DMUI_IMAGE_STATUS_READY ||
				image->deviceGeneration != deviceGeneration ||
				service.deviceGeneration != deviceGeneration ||
				service.device != device.Get() ||
				image->provenance != ImageProvenance::kCpuPixels ||
				!image->view)
				return DMUI_RESULT_STALE_HANDLE;
			previous = std::move(image->view);
			image->view = std::move(replacement);
			image->width = a_descriptor->width;
			image->height = a_descriptor->height;
		}
		return DMUI_RESULT_OK;
	}

	DMUI_Result DrawImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image,
		const DMUI_ImageDrawOptions* a_options) noexcept
	{
		if (!a_options || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_image == DMUI_INVALID_IMAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_options->structSize < DMUI_IMAGE_DRAW_OPTIONS_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!RenderExecution::IsActiveClient(a_client, true))
			return DMUI_RESULT_WRONG_THREAD;
		if (!std::isfinite(a_options->size.x) ||
			!std::isfinite(a_options->size.y) ||
			!std::isfinite(a_options->uv0.x) ||
			!std::isfinite(a_options->uv0.y) ||
			!std::isfinite(a_options->uv1.x) ||
			!std::isfinite(a_options->uv1.y))
			return DMUI_RESULT_INVALID_ARGUMENT;

		ID3D11ShaderResourceView* view{};
		uint32_t width{};
		uint32_t height{};
		{
			auto& service = GetImageService();
			const std::scoped_lock lock{ service.mutex };
			auto* image = FindImage(service, a_image);
			if (!image || image->owner != a_client)
				return DMUI_RESULT_STALE_HANDLE;
			if (image->status != DMUI_IMAGE_STATUS_READY ||
				image->deviceGeneration != service.deviceGeneration ||
				!image->view)
				return DMUI_RESULT_STALE_HANDLE;
			view = image->view.Get();
			width = image->width;
			height = image->height;
			try
			{
				service.frameLeases.push_back(image->view);
			}
			catch (const std::bad_alloc&)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
		}

		auto size = ImVec2{ a_options->size.x, a_options->size.y };
		if (size.x <= 0.0f && size.y <= 0.0f)
			size = { static_cast<float>(width), static_cast<float>(height) };
		else if (a_options->preserveAspect)
		{
			const auto aspect = static_cast<float>(width) /
				static_cast<float>(height);
			if (size.x <= 0.0f)
				size.x = size.y * aspect;
			else if (size.y <= 0.0f)
				size.y = size.x / aspect;
			else
			{
				const auto fittedHeight = size.x / aspect;
				if (fittedHeight <= size.y)
					size.y = fittedHeight;
				else
					size.x = size.y * aspect;
			}
		}
		if (size.x <= 0.0f || size.y <= 0.0f)
			return DMUI_RESULT_INVALID_ARGUMENT;
		ImGui::ImageWithBg(
			ImTextureRef{
				static_cast<ImTextureID>(
					reinterpret_cast<uintptr_t>(view))
			},
			size,
			{ a_options->uv0.x, a_options->uv0.y },
			{ a_options->uv1.x, a_options->uv1.y },
			{},
			{
				a_options->tint.x,
				a_options->tint.y,
				a_options->tint.z,
				a_options->tint.w
			});
		return DMUI_RESULT_OK;
	}

	DMUI_Result ReleaseImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_image == DMUI_INVALID_IMAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto& service = GetImageService();
		const std::scoped_lock lock{ service.mutex };
		auto* image = FindImage(service, a_image);
		if (!image || image->owner != a_client ||
			image->status == DMUI_IMAGE_STATUS_RELEASED)
			return DMUI_RESULT_STALE_HANDLE;
		const auto slot = static_cast<uint32_t>(a_image) - 1u;
		RecycleImage(service, slot, DMUI_IMAGE_STATUS_RELEASED);
		return DMUI_RESULT_OK;
	}

	DMUI_Result QueryImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image,
		DMUI_ImageInfo* a_info) noexcept
	{
		if (!a_info || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_image == DMUI_INVALID_IMAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_info->structSize < DMUI_IMAGE_INFO_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		auto& service = GetImageService();
		const std::scoped_lock lock{ service.mutex };
		auto* image = FindImage(service, a_image);
		if (!image || image->owner != a_client)
			return DMUI_RESULT_STALE_HANDLE;
		a_info->status = image->status;
		a_info->contentWidth = image->width;
		a_info->contentHeight = image->height;
		a_info->deviceGeneration = image->deviceGeneration;
		return DMUI_RESULT_OK;
	}

#if defined(DMUI_UI_TESTING)
	size_t ImageSlotCount() noexcept
	{
		auto& service = GetImageService();
		const std::scoped_lock lock{ service.mutex };
		return service.images.size();
	}

	ID3D11ShaderResourceView* RetainImageViewForTests(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image) noexcept
	{
		auto& service = GetImageService();
		const std::scoped_lock lock{ service.mutex };
		auto* image = FindImage(service, a_image);
		if (!image || image->owner != a_client ||
			image->status != DMUI_IMAGE_STATUS_READY ||
			!image->view)
			return nullptr;
		auto retained = image->view;
		return retained.Detach();
	}
#endif

	namespace ImageResources
	{
		void SetDevice(ID3D11Device* a_device) noexcept
		{
			auto& service = GetImageService();
			const std::scoped_lock lock{ service.mutex };
			if (service.device == a_device)
				return;
			service.device = a_device;
			++service.deviceGeneration;
			service.frameLeases.clear();
			for (uint32_t slot = 0; slot < service.images.size(); ++slot)
			{
				auto& image = service.images[slot];
				if (image.status == DMUI_IMAGE_STATUS_READY)
					RecycleImage(
						service, slot, DMUI_IMAGE_STATUS_INVALIDATED);
			}
		}

		void ReleaseFrameLeases() noexcept
		{
			auto& service = GetImageService();
			const std::scoped_lock lock{ service.mutex };
			service.frameLeases.clear();
		}

		bool HasDevice() noexcept
		{
			auto& service = GetImageService();
			const std::scoped_lock lock{ service.mutex };
			return service.device != nullptr;
		}

		uint64_t DeviceGeneration() noexcept
		{
			auto& service = GetImageService();
			const std::scoped_lock lock{ service.mutex };
			return service.deviceGeneration;
		}
	}
}
