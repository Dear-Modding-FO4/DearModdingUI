#include <DearModdingUI/PresentationServices.h>
#include "MenuDismissal.h"

#include <d3d11.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace DearModdingUI::PresentationServices
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		inline constexpr size_t kNotificationCapacity{ 1024 };
		inline constexpr size_t kDialogTextLimit{ 4096 };
		inline constexpr size_t kDialogStringLimit{ 1024 };
		inline constexpr size_t kPlotSampleLimit{ 1'000'000 };
		inline constexpr size_t kPlotReferenceLimit{ 64 };
		inline constexpr uint32_t kNoImageSlot{
			(std::numeric_limits<uint32_t>::max)()
		};

		thread_local DMUI_ClientHandle s_activeClient{ DMUI_INVALID_CLIENT_HANDLE };
		thread_local bool s_drawingCallback{ false };

		struct ImageEntry
		{
			DMUI_ClientHandle owner{ DMUI_INVALID_CLIENT_HANDLE };
			ID3D11ShaderResourceView* view{ nullptr };
			uint32_t width{};
			uint32_t height{};
			uint64_t deviceGeneration{};
			uint32_t handleGeneration{ 1 };
			uint32_t nextFree{ kNoImageSlot };
			DMUI_ImageStatus status{ DMUI_IMAGE_STATUS_READY };
			bool reusable{};
		};

		struct OverlayEntry
		{
			DMUI_ClientHandle owner{ DMUI_INVALID_CLIENT_HANDLE };
			DMUI_PageHandle page{ DMUI_INVALID_PAGE_HANDLE };
			DMUI_ManagedOverlayOptions options{};
			DMUI_ManagedOverlayPlacement placement{};
			bool configured{};
			bool arrangementInProgress{};
		};

		struct Notification
		{
			DMUI_ClientHandle owner{ DMUI_INVALID_CLIENT_HANDLE };
			DMUI_StatusSeverity severity{ DMUI_STATUS_SEVERITY_INFO };
			std::string message;
			Clock::time_point expiresAt{};
			uint64_t generation{};
		};

		struct Dialog
		{
			DMUI_DialogHandle handle{ DMUI_INVALID_DIALOG_HANDLE };
			DMUI_ClientHandle owner{ DMUI_INVALID_CLIENT_HANDLE };
			DMUI_DialogKind kind{ DMUI_DIALOG_KIND_CONFIRM };
			std::string title;
			std::string body;
			std::string acceptLabel;
			std::string cancelLabel;
			std::string hint;
			std::vector<char> text;
			std::string error;
			DMUI_DialogEventKind event{ DMUI_DIALOG_EVENT_PENDING };
			uint64_t submissionId{};
			bool popupOpened{};
			uint32_t popupId{};
		};

		struct Service
		{
			std::mutex mutex;
			std::thread::id renderThread;
			ID3D11Device* device{};
			uint64_t deviceGeneration{ 1 };
			DMUI_DialogHandle nextDialog{ 1 };
			uint64_t nextSubmission{ 1 };
			uint64_t nextNotificationGeneration{ 1 };
			std::vector<ImageEntry> images;
			uint32_t firstFreeImage{ kNoImageSlot };
			std::vector<ID3D11ShaderResourceView*> frameLeases;
			std::vector<OverlayEntry> overlays;
			Notification notification;
			Dialog dialog;
		};

		[[nodiscard]] Service& GetService() noexcept
		{
			static Service service;
			return service;
		}

		[[nodiscard]] DMUI_Result SubmitDialogLocked(
			Service& a_service,
			Dialog& a_dialog,
			DMUI_DialogHandle a_handle) noexcept
		{
			if (a_dialog.handle != a_handle)
				return DMUI_RESULT_STALE_HANDLE;
			if (a_dialog.event != DMUI_DIALOG_EVENT_PENDING)
				return a_dialog.event == DMUI_DIALOG_EVENT_SUBMITTED ?
					DMUI_RESULT_BUSY :
					DMUI_RESULT_STALE_HANDLE;
			if (a_service.nextSubmission == 0)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			a_dialog.event = DMUI_DIALOG_EVENT_SUBMITTED;
			a_dialog.submissionId = a_service.nextSubmission++;
			a_dialog.error.clear();
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] bool ValidSeverity(DMUI_StatusSeverity a_severity) noexcept
		{
			return a_severity <= DMUI_STATUS_SEVERITY_ERROR;
		}

		[[nodiscard]] bool ReadString(
			const char* a_value,
			size_t a_limit,
			bool a_optional,
			std::string& a_output)
		{
			if (!a_value)
			{
				if (a_optional)
				return true;
				return false;
			}
			size_t length{};
			while (length <= a_limit && a_value[length])
				++length;
			if (length > a_limit || (!a_optional && !length))
				return false;
			a_output.assign(a_value, length);
			return true;
		}

		void ReleaseView(ID3D11ShaderResourceView*& a_view) noexcept
		{
			if (a_view)
				a_view->Release();
			a_view = nullptr;
		}

		void ReleaseLeases(std::vector<ID3D11ShaderResourceView*>& a_leases) noexcept
		{
			for (auto*& view : a_leases)
				ReleaseView(view);
			a_leases.clear();
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
			case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_R16G16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32G32_FLOAT:
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
				return true;
			default:
				return false;
			}
		}

		[[nodiscard]] ImageEntry* FindImage(
			Service& a_service,
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
			Service& a_service,
			uint32_t a_slot,
			DMUI_ImageStatus a_status) noexcept
		{
			auto& image = a_service.images[a_slot];
			ReleaseView(image.view);
			image.status = a_status;
			if (!image.reusable &&
				image.handleGeneration !=
					(std::numeric_limits<uint32_t>::max)())
			{
				image.nextFree = a_service.firstFreeImage;
				image.reusable = true;
				a_service.firstFreeImage = a_slot;
			}
		}

		[[nodiscard]] OverlayEntry* FindOverlay(
			Service& a_service,
			DMUI_PageHandle a_page) noexcept
		{
			const auto found = std::ranges::find(
				a_service.overlays, a_page, &OverlayEntry::page);
			return found == a_service.overlays.end() ? nullptr : &*found;
		}

		[[nodiscard]] ImVec4 NotificationColor(
			DMUI_StatusSeverity a_severity) noexcept
		{
			switch (a_severity)
			{
			case DMUI_STATUS_SEVERITY_SUCCESS:
				return { 0.20f, 0.70f, 0.38f, 0.96f };
			case DMUI_STATUS_SEVERITY_WARNING:
				return { 0.90f, 0.62f, 0.15f, 0.96f };
			case DMUI_STATUS_SEVERITY_ERROR:
				return { 0.88f, 0.24f, 0.22f, 0.96f };
			default:
				return { 0.18f, 0.55f, 0.88f, 0.96f };
			}
		}

		[[nodiscard]] ImVec2 ResolveOverlaySize(
			const DMUI_ManagedOverlayOptions& a_options,
			float a_scale) noexcept
		{
			const auto scaled = [a_scale](float a_value) {
				return a_value > 0.0f ? a_value * a_scale : 0.0f;
			};
			return {
				scaled(a_options.minimumSize.x),
				scaled(a_options.minimumSize.y)
			};
		}

		[[nodiscard]] ImVec2 ResolveOverlayPosition(
			const DMUI_ManagedOverlayOptions& a_options,
			ImVec2 a_viewport,
			ImVec2 a_size,
			float a_scale) noexcept
		{
			const ImVec2 offset{
				a_options.offset.x * a_scale,
				a_options.offset.y * a_scale
			};
			switch (a_options.anchor)
			{
			case DMUI_OVERLAY_ANCHOR_TOP_RIGHT:
				return { a_viewport.x - a_size.x - offset.x, offset.y };
			case DMUI_OVERLAY_ANCHOR_BOTTOM_LEFT:
				return { offset.x, a_viewport.y - a_size.y - offset.y };
			case DMUI_OVERLAY_ANCHOR_BOTTOM_RIGHT:
				return {
					a_viewport.x - a_size.x - offset.x,
					a_viewport.y - a_size.y - offset.y
				};
			default:
				return offset;
			}
		}

		[[nodiscard]] bool IsRenderThread(const Service& a_service) noexcept
		{
			return a_service.renderThread != std::thread::id{} &&
				a_service.renderThread == std::this_thread::get_id();
		}
	}

	ClientExecutionGuard::ClientExecutionGuard(
		DMUI_ClientHandle a_client,
		bool a_drawing) noexcept :
		m_previousClient(s_activeClient),
		m_previousDrawing(s_drawingCallback)
	{
		s_activeClient = a_client;
		s_drawingCallback = a_drawing;
	}

	ClientExecutionGuard::~ClientExecutionGuard() noexcept
	{
		s_activeClient = m_previousClient;
		s_drawingCallback = m_previousDrawing;
	}

	void BindRenderThread() noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		if (service.renderThread == std::thread::id{})
			service.renderThread = std::this_thread::get_id();
	}

	void BindRenderer(ID3D11Device* a_device) noexcept
	{
		BindRenderThread();
		SetDevice(a_device);
	}

	void SetDevice(ID3D11Device* a_device) noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		if (service.device == a_device)
			return;
		service.device = a_device;
		++service.deviceGeneration;
		ReleaseLeases(service.frameLeases);
		for (uint32_t slot = 0; slot < service.images.size(); ++slot)
		{
			auto& image = service.images[slot];
			if (image.status == DMUI_IMAGE_STATUS_READY)
				RecycleImage(
					service, slot, DMUI_IMAGE_STATUS_INVALIDATED);
		}
	}

	void InvalidateDevice() noexcept
	{
		SetDevice(nullptr);
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		service.notification = {};
	}

	void BeginFrame() noexcept
	{
		BindRenderThread();
		DiscardFrame();
	}

	void CompleteRenderSubmission() noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		ReleaseLeases(service.frameLeases);
	}

	void DiscardFrame() noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		ReleaseLeases(service.frameLeases);
	}

	bool IsActiveClient(
		DMUI_ClientHandle a_client,
		bool a_drawingRequired) noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		return IsRenderThread(service) &&
			s_activeClient == a_client &&
			(!a_drawingRequired || s_drawingCallback);
	}

	uint64_t DeviceGeneration() noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		return service.deviceGeneration;
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
			auto& service = GetService();
			const std::scoped_lock lock{ service.mutex };
			if (!IsRenderThread(service))
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
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		const auto deviceMatches = imageDevice && imageDevice == service.device;
		if (imageDevice)
			imageDevice->Release();
		if (!deviceMatches || !service.device)
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
		uint32_t slot{ kNoImageSlot };
		while (service.firstFreeImage != kNoImageSlot)
		{
			slot = service.firstFreeImage;
			auto& candidate = service.images[slot];
			service.firstFreeImage = candidate.nextFree;
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

		view->AddRef();
		if (slot == kNoImageSlot)
		{
			if (service.images.size() >=
				(std::numeric_limits<uint32_t>::max)())
			{
				view->Release();
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
			try
			{
				service.images.push_back({
					a_client,
					view,
					width,
					height,
					service.deviceGeneration
				});
				slot = static_cast<uint32_t>(
					service.images.size() - 1u);
			}
			catch (...)
			{
				view->Release();
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
		}
		else
		{
			auto& image = service.images[slot];
			image.owner = a_client;
			image.view = view;
			image.width = width;
			image.height = height;
			image.deviceGeneration = service.deviceGeneration;
			image.status = DMUI_IMAGE_STATUS_READY;
		}
		*a_image = MakeImageHandle(
			slot, service.images[slot].handleGeneration);
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
		if (!IsActiveClient(a_client, true))
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
			auto& service = GetService();
			const std::scoped_lock lock{ service.mutex };
			auto* image = FindImage(service, a_image);
			if (!image || image->owner != a_client)
				return DMUI_RESULT_STALE_HANDLE;
			if (image->status != DMUI_IMAGE_STATUS_READY ||
				image->deviceGeneration != service.deviceGeneration ||
				!image->view)
				return DMUI_RESULT_STALE_HANDLE;
			view = image->view;
			width = image->width;
			height = image->height;
			try
			{
				view->AddRef();
				service.frameLeases.push_back(view);
			}
			catch (...)
			{
				view->Release();
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
		auto& service = GetService();
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
		auto& service = GetService();
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

	size_t ImageSlotCount() noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		return service.images.size();
	}

	DMUI_Result ConfigureOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		const DMUI_ManagedOverlayOptions* a_options) noexcept
	{
		if (!a_options || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_page == DMUI_INVALID_PAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_options->structSize < DMUI_MANAGED_OVERLAY_OPTIONS_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (a_options->anchor > DMUI_OVERLAY_ANCHOR_FREE ||
			!std::isfinite(a_options->offset.x) ||
			!std::isfinite(a_options->offset.y) ||
			!std::isfinite(a_options->minimumSize.x) ||
			!std::isfinite(a_options->minimumSize.y) ||
			!std::isfinite(a_options->maximumSize.x) ||
			!std::isfinite(a_options->maximumSize.y) ||
			!std::isfinite(a_options->opacity) ||
			!std::isfinite(a_options->contentScale) ||
			a_options->opacity < 0.0f ||
			a_options->opacity > 1.0f ||
			a_options->contentScale < 0.5f ||
			a_options->contentScale > 3.0f ||
			a_options->minimumSize.x < 0.0f ||
			a_options->minimumSize.y < 0.0f ||
			(a_options->maximumSize.x > 0.0f &&
				a_options->maximumSize.x < a_options->minimumSize.x) ||
			(a_options->maximumSize.y > 0.0f &&
				a_options->maximumSize.y < a_options->minimumSize.y))
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			auto& service = GetService();
			const std::scoped_lock lock{ service.mutex };
			auto* overlay = FindOverlay(service, a_page);
			if (overlay && overlay->owner != a_client)
				return DMUI_RESULT_PAGE_NOT_FOUND;
			if (!overlay)
			{
				service.overlays.push_back({});
				overlay = &service.overlays.back();
				overlay->owner = a_client;
				overlay->page = a_page;
				overlay->placement.structSize =
					sizeof(DMUI_ManagedOverlayPlacement);
			}
			overlay->options = *a_options;
			overlay->configured = true;
			overlay->placement.anchor = a_options->anchor;
			overlay->placement.offset = a_options->offset;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result QueryOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		DMUI_ManagedOverlayPlacement* a_placement) noexcept
	{
		if (!a_placement || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_page == DMUI_INVALID_PAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_placement->structSize < DMUI_MANAGED_OVERLAY_PLACEMENT_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		const auto* overlay = FindOverlay(service, a_page);
		if (!overlay || overlay->owner != a_client || !overlay->configured)
			return DMUI_RESULT_PAGE_NOT_FOUND;
		*a_placement = overlay->placement;
		return DMUI_RESULT_OK;
	}

	ManagedOverlayBeginResult BeginManagedOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		std::string_view a_label,
		bool a_menuVisible) noexcept
	{
		DMUI_ManagedOverlayOptions options{};
		DMUI_ManagedOverlayPlacement previous{};
		{
			auto& service = GetService();
			const std::scoped_lock lock{ service.mutex };
			const auto* overlay = FindOverlay(service, a_page);
			if (!overlay || overlay->owner != a_client || !overlay->configured)
				return ManagedOverlayBeginResult::kNotConfigured;
			options = overlay->options;
			previous = overlay->placement;
		}

		const auto& io = ImGui::GetIO();
		const auto hostScale = io.FontGlobalScale > 0.0f ?
			io.FontGlobalScale :
			1.0f;
		const auto scale = hostScale * options.contentScale;
		const auto viewport = io.DisplaySize;
		auto expectedSize = ResolveOverlaySize(options, scale);
		if (expectedSize.x <= 0.0f)
			expectedSize.x = previous.size.x;
		if (expectedSize.y <= 0.0f)
			expectedSize.y = previous.size.y;
		const auto position = ResolveOverlayPosition(
			options,
			viewport,
			expectedSize,
			hostScale);
		const auto anchored = options.anchor != DMUI_OVERLAY_ANCHOR_FREE;
		if (anchored || previous.changeGeneration == 0)
			ImGui::SetNextWindowPos(position, ImGuiCond_Always);
		if (options.minimumSize.x > 0.0f || options.minimumSize.y > 0.0f)
			ImGui::SetNextWindowSize(expectedSize, ImGuiCond_FirstUseEver);
		const ImVec2 minimum{
			options.minimumSize.x > 0.0f ?
				options.minimumSize.x * scale :
				0.0f,
			options.minimumSize.y > 0.0f ?
				options.minimumSize.y * scale :
				0.0f
		};
		const ImVec2 maximum{
			options.maximumSize.x > 0.0f ?
				options.maximumSize.x * scale :
				(std::numeric_limits<float>::max)(),
			options.maximumSize.y > 0.0f ?
				options.maximumSize.y * scale :
				(std::numeric_limits<float>::max)()
		};
		ImGui::SetNextWindowSizeConstraints(minimum, maximum);
		ImGui::SetNextWindowBgAlpha(options.opacity);
		auto flags =
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav;
		if (!a_menuVisible || !options.allowArrangement)
			flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
		if (anchored)
			flags |= ImGuiWindowFlags_NoMove;
		if (!options.backgroundVisible)
			flags |= ImGuiWindowFlags_NoBackground;
		if (!options.borderVisible)
			flags |= ImGuiWindowFlags_NoDecoration;
		const std::string windowLabel{
			a_label.empty() ? "Managed overlay" : a_label
		};
		const auto opened = ImGui::Begin(windowLabel.c_str(), nullptr, flags);
		ImGui::SetWindowFontScale(options.contentScale);

		const auto currentPosition = ImGui::GetWindowPos();
		const auto currentSize = ImGui::GetWindowSize();
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		auto* overlay = FindOverlay(service, a_page);
		if (overlay)
		{
			const auto changed =
				overlay->placement.position.x != currentPosition.x ||
				overlay->placement.position.y != currentPosition.y ||
				overlay->placement.size.x != currentSize.x ||
				overlay->placement.size.y != currentSize.y;
			overlay->placement.position = {
				currentPosition.x,
				currentPosition.y
			};
			overlay->placement.size = { currentSize.x, currentSize.y };
			overlay->placement.visible = 1u;
			if (changed)
				++overlay->placement.changeGeneration;
			const auto arrangementEnabled =
				a_menuVisible &&
				options.allowArrangement &&
				!anchored;
			if (arrangementEnabled && changed &&
				ImGui::IsMouseDown(ImGuiMouseButton_Left))
				overlay->arrangementInProgress = true;
			overlay->placement.arrangementCompleted =
				arrangementEnabled &&
				overlay->arrangementInProgress &&
				ImGui::IsMouseReleased(ImGuiMouseButton_Left) ?
				1u :
				0u;
			if (overlay->placement.arrangementCompleted)
				overlay->arrangementInProgress = false;
		}
		return opened ?
			ManagedOverlayBeginResult::kVisible :
			ManagedOverlayBeginResult::kHidden;
	}

	void EndManagedOverlay() noexcept
	{
		ImGui::End();
	}

	DMUI_Result PostNotification(
		DMUI_ClientHandle a_client,
		const DMUI_NotificationDescriptor* a_descriptor) noexcept
	{
		if (!a_descriptor || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_descriptor->structSize < DMUI_NOTIFICATION_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!ValidSeverity(a_descriptor->severity))
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			std::string message;
			if (!ReadString(
					a_descriptor->message,
					kNotificationCapacity,
					false,
					message))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			const auto duration = std::chrono::milliseconds{
				(std::clamp)(
					a_descriptor->durationMilliseconds ?
						a_descriptor->durationMilliseconds :
						4000u,
					250u,
					30000u)
			};
			auto& service = GetService();
			const std::scoped_lock lock{ service.mutex };
			service.notification = {
				a_client,
				a_descriptor->severity,
				std::move(message),
				Clock::now() + duration,
				service.nextNotificationGeneration++
			};
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	void DrawNotification() noexcept
	{
		Notification notification;
		{
			auto& service = GetService();
			const std::scoped_lock lock{ service.mutex };
			if (service.notification.message.empty())
				return;
			if (Clock::now() >= service.notification.expiresAt)
			{
				service.notification = {};
				return;
			}
			try
			{
				notification = service.notification;
			}
			catch (...)
			{
				return;
			}
		}
		const auto& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(
			{ io.DisplaySize.x * 0.5f, 24.0f },
			ImGuiCond_Always,
			{ 0.5f, 0.0f });
		ImGui::SetNextWindowBgAlpha(0.94f);
		const auto flags =
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoSavedSettings;
		if (ImGui::Begin("Notification###dmui.notification", nullptr, flags))
			ImGui::TextColored(
				NotificationColor(notification.severity),
				"%s",
				notification.message.c_str());
		ImGui::End();
	}

	DMUI_Result DrawAnnotatedPlot(
		DMUI_ClientHandle a_client,
		const char* a_id,
		const DMUI_AnnotatedPlotDescriptor* a_descriptor) noexcept
	{
		if (!a_id || !*a_id || !a_descriptor ||
			a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_descriptor->structSize < DMUI_ANNOTATED_PLOT_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!IsActiveClient(a_client, true))
			return DMUI_RESULT_WRONG_THREAD;
		if (a_descriptor->sampleCount > kPlotSampleLimit ||
			a_descriptor->referenceLineCount > kPlotReferenceLimit ||
			(a_descriptor->sampleCount && !a_descriptor->samples) ||
			(a_descriptor->referenceLineCount && !a_descriptor->referenceLines) ||
			!std::isfinite(a_descriptor->scaleMinimum) ||
			!std::isfinite(a_descriptor->scaleMaximum) ||
			a_descriptor->scaleMaximum <= a_descriptor->scaleMinimum ||
			!std::isfinite(a_descriptor->size.x) ||
			!std::isfinite(a_descriptor->size.y))
			return DMUI_RESULT_INVALID_ARGUMENT;
		for (size_t index = 0; index < a_descriptor->sampleCount; ++index)
			if (!std::isfinite(a_descriptor->samples[index]))
				return DMUI_RESULT_INVALID_ARGUMENT;
		for (size_t index = 0; index < a_descriptor->referenceLineCount; ++index)
			if (!std::isfinite(a_descriptor->referenceLines[index].value))
				return DMUI_RESULT_INVALID_ARGUMENT;

		const auto sampleCount = static_cast<int>(a_descriptor->sampleCount);
		const auto sampleOffset = sampleCount ?
			static_cast<int>(a_descriptor->sampleOffset %
				a_descriptor->sampleCount) :
			0;
		const auto labelSize = ImGui::CalcTextSize(a_id, nullptr, true);
		const auto frameSize = ImGui::CalcItemSize(
			{ a_descriptor->size.x, a_descriptor->size.y },
			ImGui::CalcItemWidth(),
			labelSize.y + ImGui::GetStyle().FramePadding.y * 2.0f);
		ImGui::PlotLines(
			a_id,
			a_descriptor->samples,
			sampleCount,
			sampleOffset,
			a_descriptor->overlayText,
			a_descriptor->scaleMinimum,
			a_descriptor->scaleMaximum,
			{ a_descriptor->size.x, a_descriptor->size.y });
		const auto itemMinimum = ImGui::GetItemRectMin();
		const auto padding = ImGui::GetStyle().FramePadding;
		const ImVec2 plotMinimum{
			itemMinimum.x + padding.x,
			itemMinimum.y + padding.y
		};
		const ImVec2 plotMaximum{
			itemMinimum.x + frameSize.x - padding.x,
			itemMinimum.y + frameSize.y - padding.y
		};
		auto* drawList = ImGui::GetWindowDrawList();
		drawList->PushClipRect(plotMinimum, plotMaximum, true);
		for (size_t index = 0; index < a_descriptor->referenceLineCount; ++index)
		{
			const auto& line = a_descriptor->referenceLines[index];
			if (line.value < a_descriptor->scaleMinimum ||
				line.value > a_descriptor->scaleMaximum)
				continue;
			const auto ratio =
				(line.value - a_descriptor->scaleMinimum) /
				(a_descriptor->scaleMaximum - a_descriptor->scaleMinimum);
			const auto y =
				plotMaximum.y -
				ratio * (plotMaximum.y - plotMinimum.y);
			drawList->AddLine(
				{ plotMinimum.x, y },
				{ plotMaximum.x, y },
				ImGui::ColorConvertFloat4ToU32({
					line.color.x,
					line.color.y,
					line.color.z,
					line.color.w
				}));
		}
		drawList->PopClipRect();
		return DMUI_RESULT_OK;
	}

	DMUI_Result RequestDialog(
		DMUI_ClientHandle a_client,
		const DMUI_DialogDescriptor* a_descriptor,
		DMUI_DialogHandle* a_dialog,
		bool a_menuVisible) noexcept
	{
		if (!a_descriptor || !a_dialog ||
			a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_dialog = DMUI_INVALID_DIALOG_HANDLE;
		if (a_descriptor->structSize < DMUI_DIALOG_DESCRIPTOR_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!a_menuVisible)
			return DMUI_RESULT_NOT_VISIBLE;
		if (!IsActiveClient(a_client, false))
			return DMUI_RESULT_WRONG_THREAD;
		if (a_descriptor->kind != DMUI_DIALOG_KIND_CONFIRM &&
			a_descriptor->kind != DMUI_DIALOG_KIND_TEXT_ENTRY)
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			Dialog dialog;
			if (!ReadString(
					a_descriptor->title,
					kDialogStringLimit,
					false,
					dialog.title) ||
				!ReadString(
					a_descriptor->body,
					kDialogStringLimit,
					true,
					dialog.body) ||
				!ReadString(
					a_descriptor->acceptLabel,
					64,
					false,
					dialog.acceptLabel) ||
				!ReadString(
					a_descriptor->cancelLabel,
					64,
					false,
					dialog.cancelLabel) ||
				!ReadString(
					a_descriptor->hint,
					kDialogStringLimit,
					true,
					dialog.hint))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			const auto maximum =
				a_descriptor->kind == DMUI_DIALOG_KIND_TEXT_ENTRY ?
					a_descriptor->maximumTextBytes :
					1u;
			if (!maximum || maximum > kDialogTextLimit)
				return DMUI_RESULT_INVALID_ARGUMENT;
			std::string initial;
			if (!ReadString(
					a_descriptor->initialText,
					maximum - 1u,
					true,
					initial))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			dialog.text.assign(maximum, '\0');
			std::copy(initial.begin(), initial.end(), dialog.text.begin());
			dialog.owner = a_client;
			dialog.kind = a_descriptor->kind;
			auto& service = GetService();
			const std::scoped_lock lock{ service.mutex };
			if (service.dialog.handle != DMUI_INVALID_DIALOG_HANDLE)
				return DMUI_RESULT_BUSY;
			if (service.nextDialog == DMUI_INVALID_DIALOG_HANDLE)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			dialog.handle = service.nextDialog++;
			service.dialog = std::move(dialog);
			*a_dialog = service.dialog.handle;
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result PollDialogEvent(
		DMUI_ClientHandle a_client,
		DMUI_DialogHandle a_dialog,
		DMUI_DialogEvent* a_event,
		char* a_textBuffer,
		uint32_t a_textCapacity) noexcept
	{
		if (!a_event || a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_dialog == DMUI_INVALID_DIALOG_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_event->structSize < DMUI_DIALOG_EVENT_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		if (!IsActiveClient(a_client, false))
			return DMUI_RESULT_WRONG_THREAD;
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		auto& dialog = service.dialog;
		if (dialog.handle != a_dialog || dialog.owner != a_client)
			return DMUI_RESULT_STALE_HANDLE;
		const auto textLength = dialog.text.empty() ?
			0u :
			std::strlen(dialog.text.data());
		const auto required = static_cast<uint32_t>(textLength + 1u);
		a_event->kind = dialog.event;
		a_event->submissionId = dialog.submissionId;
		a_event->requiredTextCapacity = required;
		if ((dialog.event == DMUI_DIALOG_EVENT_SUBMITTED ||
				dialog.kind == DMUI_DIALOG_KIND_TEXT_ENTRY) &&
			(!a_textBuffer || a_textCapacity < required))
			return DMUI_RESULT_BUFFER_TOO_SMALL;
		if (a_textBuffer && a_textCapacity)
			std::memcpy(a_textBuffer, dialog.text.data(), required);
		if (dialog.event == DMUI_DIALOG_EVENT_CANCELLED ||
			dialog.event == DMUI_DIALOG_EVENT_COMPLETED)
			dialog = {};
		return DMUI_RESULT_OK;
	}

	DMUI_Result ResolveDialogSubmission(
		DMUI_ClientHandle a_client,
		DMUI_DialogHandle a_dialog,
		uint64_t a_submissionId,
		uint32_t a_accepted,
		const char* a_error) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_dialog == DMUI_INVALID_DIALOG_HANDLE ||
			!a_submissionId)
			return DMUI_RESULT_INVALID_ARGUMENT;
		try
		{
			std::string error;
			if (!a_accepted &&
				!ReadString(a_error, kDialogStringLimit, true, error))
				return DMUI_RESULT_INVALID_DESCRIPTOR;
			auto& service = GetService();
			const std::scoped_lock lock{ service.mutex };
			auto& dialog = service.dialog;
			if (dialog.handle != a_dialog || dialog.owner != a_client)
				return DMUI_RESULT_STALE_HANDLE;
			if (dialog.event != DMUI_DIALOG_EVENT_SUBMITTED ||
				dialog.submissionId != a_submissionId)
				return DMUI_RESULT_STALE_SUBMISSION;
			if (a_accepted)
			{
				dialog.event = DMUI_DIALOG_EVENT_COMPLETED;
				dialog.error.clear();
			}
			else
			{
				dialog.event = DMUI_DIALOG_EVENT_PENDING;
				dialog.error = std::move(error);
			}
			return DMUI_RESULT_OK;
		}
		catch (...)
		{
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		}
	}

	DMUI_Result CancelDialog(
		DMUI_ClientHandle a_client,
		DMUI_DialogHandle a_dialog) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE ||
			a_dialog == DMUI_INVALID_DIALOG_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		auto& dialog = service.dialog;
		if (dialog.handle != a_dialog || dialog.owner != a_client)
			return DMUI_RESULT_STALE_HANDLE;
		if (dialog.event == DMUI_DIALOG_EVENT_SUBMITTED)
			return DMUI_RESULT_BUSY;
		dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
		return DMUI_RESULT_OK;
	}

	DMUI_Result SubmitDialog(DMUI_DialogHandle a_dialog) noexcept
	{
		if (a_dialog == DMUI_INVALID_DIALOG_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		return SubmitDialogLocked(service, service.dialog, a_dialog);
	}

	void DrawDialog(bool a_menuVisible) noexcept
	{
		try
		{
			const auto escapeDismissed =
				DismissCapturedMenuDialog();
			auto& service = GetService();
			Dialog snapshot;
			bool openPopup{};
			{
				const std::scoped_lock lock{ service.mutex };
				auto& dialog = service.dialog;
				if (dialog.handle == DMUI_INVALID_DIALOG_HANDLE)
					return;
				if (!a_menuVisible)
				{
					if (dialog.event == DMUI_DIALOG_EVENT_PENDING)
						dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
					return;
				}
				if (dialog.event == DMUI_DIALOG_EVENT_COMPLETED ||
					dialog.event == DMUI_DIALOG_EVENT_CANCELLED)
					return;
				snapshot = dialog;
				openPopup = !dialog.popupOpened;
				dialog.popupOpened = true;
			}

			const auto popupId = snapshot.title + "###dmui.dialog";
			const auto popupImGuiId = ImGui::GetID(popupId.c_str());
			{
				const std::scoped_lock lock{ service.mutex };
				if (service.dialog.handle == snapshot.handle)
					service.dialog.popupId = popupImGuiId;
			}
			if (openPopup)
				ImGui::OpenPopup(popupId.c_str());
			bool open{ true };
			if (!ImGui::BeginPopupModal(
					popupId.c_str(),
					snapshot.event == DMUI_DIALOG_EVENT_PENDING ?
						&open :
						nullptr,
					ImGuiWindowFlags_AlwaysAutoResize))
			{
				if (escapeDismissed &&
					snapshot.event == DMUI_DIALOG_EVENT_PENDING)
				{
					const std::scoped_lock lock{ service.mutex };
					if (service.dialog.handle == snapshot.handle &&
						service.dialog.event == DMUI_DIALOG_EVENT_PENDING)
						service.dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
				}
				return;
			}
			if (!snapshot.body.empty())
				ImGui::TextWrapped("%s", snapshot.body.c_str());
			if (snapshot.kind == DMUI_DIALOG_KIND_TEXT_ENTRY)
			{
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 24.0f);
				(void)ImGui::InputTextWithHint(
					"##dmui.dialog.text",
					snapshot.hint.c_str(),
					snapshot.text.data(),
					snapshot.text.size());
			}
			if (!snapshot.error.empty())
				ImGui::TextColored(
					{ 0.88f, 0.24f, 0.22f, 1.0f },
					"%s",
					snapshot.error.c_str());

			enum class DialogAction
			{
				kNone,
				kSubmit,
				kCancel
			};
			auto action = DialogAction::kNone;
			if (snapshot.event == DMUI_DIALOG_EVENT_SUBMITTED)
				ImGui::TextDisabled("Working...");
			else
			{
				if (ImGui::Button(snapshot.acceptLabel.c_str()))
					action = DialogAction::kSubmit;
				ImGui::SameLine();
				if (ImGui::Button(snapshot.cancelLabel.c_str()))
					action = DialogAction::kCancel;
			}
			if (!open && snapshot.event == DMUI_DIALOG_EVENT_PENDING)
				action = DialogAction::kCancel;
			if (escapeDismissed)
			{
				if (snapshot.event == DMUI_DIALOG_EVENT_PENDING)
					action = DialogAction::kCancel;
			}
			else if (action == DialogAction::kCancel)
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();

			const std::scoped_lock lock{ service.mutex };
			auto& dialog = service.dialog;
			if (dialog.handle != snapshot.handle ||
				dialog.event != snapshot.event)
				return;
			if (dialog.kind == DMUI_DIALOG_KIND_TEXT_ENTRY &&
				dialog.event == DMUI_DIALOG_EVENT_PENDING)
				dialog.text = std::move(snapshot.text);
			if (action == DialogAction::kSubmit &&
				dialog.event == DMUI_DIALOG_EVENT_PENDING)
				(void)SubmitDialogLocked(service, dialog, dialog.handle);
			else if (action == DialogAction::kCancel &&
				dialog.event == DMUI_DIALOG_EVENT_PENDING)
				dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
		}
		catch (...)
		{
			// A presentation allocation failure leaves the dialog pending.
		}
	}

	void NotifyMenuClosed() noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		if (service.dialog.handle != DMUI_INVALID_DIALOG_HANDLE &&
			service.dialog.event == DMUI_DIALOG_EVENT_PENDING)
			service.dialog.event = DMUI_DIALOG_EVENT_CANCELLED;
	}

	bool HasFrameDemand() noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		if (!service.device)
			return false;
		if (!service.notification.message.empty() &&
			Clock::now() < service.notification.expiresAt)
			return true;
		return service.dialog.handle != DMUI_INVALID_DIALOG_HANDLE &&
			service.dialog.event != DMUI_DIALOG_EVENT_CANCELLED &&
			service.dialog.event != DMUI_DIALOG_EVENT_COMPLETED;
	}

	bool HasActiveDialog() noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		return service.dialog.handle != DMUI_INVALID_DIALOG_HANDLE &&
			service.dialog.event != DMUI_DIALOG_EVENT_CANCELLED &&
			service.dialog.event != DMUI_DIALOG_EVENT_COMPLETED;
	}

	uint32_t ActiveDialogPopupId() noexcept
	{
		auto& service = GetService();
		const std::scoped_lock lock{ service.mutex };
		if (service.dialog.handle == DMUI_INVALID_DIALOG_HANDLE ||
			service.dialog.event == DMUI_DIALOG_EVENT_CANCELLED ||
			service.dialog.event == DMUI_DIALOG_EVENT_COMPLETED)
			return 0;
		return service.dialog.popupId;
	}
}
