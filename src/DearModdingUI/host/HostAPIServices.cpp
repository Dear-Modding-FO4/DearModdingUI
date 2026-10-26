#include "HostAPIEntries.h"
#include "HostContext.h"

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/localization/Localization.h>

#include <cstring>
#include <limits>

namespace DearModdingUI::HostAPIInternal
{
	using namespace HostInternal;

	DMUI_Result DMUI_CALL ApiLoadImageFile(
		DMUI_ClientHandle a_client, const char* a_path, DMUI_ImageHandle* a_image) noexcept
	{
		const auto validation = GetService().registry.ValidateClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::LoadImageFile(a_client, a_path, a_image);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiImportD3D11Image(DMUI_ClientHandle a_client, const DMUI_D3D11ImageDescriptor *a_descriptor,
						DMUI_ImageHandle *a_image) noexcept
	{
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::ImportD3D11Image(a_client, a_descriptor, a_image);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiCreateImage(DMUI_ClientHandle a_client,
													   const DMUI_ImageDescriptor *a_descriptor,
													   DMUI_ImageHandle *a_image) noexcept
	{
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::CreateImage(a_client, a_descriptor, a_image);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiUpdateImage(DMUI_ClientHandle a_client, DMUI_ImageHandle a_image,
				   const DMUI_ImageDescriptor *a_descriptor) noexcept
	{
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::UpdateImage(a_client, a_image, a_descriptor);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiReleaseImage(DMUI_ClientHandle a_client,
														DMUI_ImageHandle a_image) noexcept
	{
		const auto clientResult = GetService().registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return PresentationServices::ReleaseImage(a_client, a_image);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryImage(DMUI_ClientHandle a_client,
													  DMUI_ImageHandle a_image,
													  DMUI_ImageInfo *a_info) noexcept
	{
		const auto clientResult = GetService().registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return PresentationServices::QueryImage(a_client, a_image, a_info);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiConfigureOverlay(DMUI_ClientHandle a_client, DMUI_PageHandle a_page,
						const DMUI_ManagedOverlayOptions *a_options) noexcept
	{
		auto &registry = GetService().registry;
		std::string clientId, pageId;
		const auto validation = registry.ValidatePage(
			a_client, a_page, DMUI_PAGE_KIND_OVERLAY, &clientId, &pageId);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::ConfigureOverlay(
			a_client, a_page, a_options, clientId, pageId);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiQueryOverlay(DMUI_ClientHandle a_client, DMUI_PageHandle a_page,
					DMUI_ManagedOverlayPlacement *a_placement) noexcept
	{
		auto &registry = GetService().registry;
		const auto validation = registry.ValidatePage(a_client, a_page, DMUI_PAGE_KIND_OVERLAY);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::QueryOverlay(a_client, a_page, a_placement);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiResetOverlay(DMUI_ClientHandle a_client, DMUI_PageHandle a_page) noexcept
	{
		auto& registry = GetService().registry;
		std::string clientId, pageId;
		const auto validation = registry.ValidatePage(
			a_client, a_page, DMUI_PAGE_KIND_OVERLAY, &clientId, &pageId);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::ResetOverlay(a_client, a_page, clientId, pageId);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiRequestOverlayFocus(DMUI_ClientHandle a_client, DMUI_PageHandle a_page) noexcept
	{
		auto& service = GetService();
		const auto validation =
			service.registry.ValidatePage(a_client, a_page, DMUI_PAGE_KIND_OVERLAY);
		if (validation != DMUI_RESULT_OK)
			return validation;
		DMUI_ManagedOverlayPlacement placement{};
		if (PresentationServices::QueryOverlay(a_client, a_page, &placement) != DMUI_RESULT_OK)
			return DMUI_RESULT_PAGE_NOT_FOUND;
		return service.overlayFocus.Request(a_page, [&]() noexcept {
			const auto state = service.state.load(std::memory_order_acquire);
			if (state != DMUI_HOST_STATE_READY)
				return StateResult(state);
			if (service.registry.PageFailed(a_page))
				return DMUI_RESULT_CALLBACK_FAILED;
			if (!service.registry.IsFrameDemanded(a_page))
				return DMUI_RESULT_NO_FRAME_DEMAND;
			return service.menuVisible.load(std::memory_order_acquire) ?
				DMUI_RESULT_BUSY : DMUI_RESULT_OK;
		});
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiReleaseOverlayFocus(DMUI_ClientHandle a_client, DMUI_PageHandle a_page) noexcept
	{
		auto& service = GetService();
		const auto validation =
			service.registry.ValidatePage(a_client, a_page, DMUI_PAGE_KIND_OVERLAY);
		if (validation == DMUI_RESULT_OK)
			service.overlayFocus.EndPage(a_page, DMUI_OVERLAY_FOCUS_END_RELEASED);
		return validation;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryOverlayFocus(
		DMUI_ClientHandle a_client, DMUI_PageHandle a_page, DMUI_OverlayFocusInfo* a_info) noexcept
	{
		if (!a_info)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto& service = GetService();
		const auto validation =
			service.registry.ValidatePage(a_client, a_page, DMUI_PAGE_KIND_OVERLAY);
		if (validation == DMUI_RESULT_OK)
			*a_info = service.overlayFocus.Query(a_page);
		return validation;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiResolveText(const char* a_owner, const char* a_key,
		char* a_buffer, uint32_t a_capacity, uint32_t* a_requiredCapacity) noexcept
	{
		if (!a_owner || !a_key || (!a_buffer && a_capacity != 0))
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (Localization::Language().empty())
			return DMUI_RESULT_HOST_NOT_READY;
		const auto* text = Localization::FindTranslation(a_owner, a_key);
		if (!text)
			return DMUI_RESULT_TEXT_NOT_FOUND;
		if (text->size() >= (std::numeric_limits<uint32_t>::max)())
			return DMUI_RESULT_RESOURCE_EXHAUSTED;
		const auto required = static_cast<uint32_t>(text->size() + 1);
		if (a_requiredCapacity)
			*a_requiredCapacity = required;
		if (a_capacity < required)
			return DMUI_RESULT_BUFFER_TOO_SMALL;
		std::memcpy(a_buffer, text->c_str(), required);
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiPostNotification(
		DMUI_ClientHandle a_client, const DMUI_NotificationDescriptor *a_descriptor) noexcept
	{
		return PresentationServices::PostNotification(GetService().registry, a_client, a_descriptor);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiRequestDialog(DMUI_ClientHandle a_client,
														 const DMUI_DialogDescriptor *a_descriptor,
														 DMUI_DialogHandle *a_dialog) noexcept
	{
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::RequestDialog(a_client, a_descriptor, a_dialog,
												   IsMenuVisible());
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiPollDialogEvent(DMUI_ClientHandle a_client,
														   DMUI_DialogHandle a_dialog,
														   DMUI_DialogEvent *a_event,
														   char *a_textBuffer,
														   uint32_t a_textCapacity) noexcept
	{
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::PollDialogEvent(a_client, a_dialog, a_event, a_textBuffer,
													 a_textCapacity);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiResolveDialogSubmission(DMUI_ClientHandle a_client,
																   DMUI_DialogHandle a_dialog,
																   uint64_t a_submissionId,
																   uint32_t a_accepted,
																   const char *a_error) noexcept
	{
		const auto clientResult = GetService().registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return PresentationServices::ResolveDialogSubmission(a_client, a_dialog, a_submissionId,
															 a_accepted, a_error);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiCancelDialog(DMUI_ClientHandle a_client,
														DMUI_DialogHandle a_dialog) noexcept
	{
		const auto clientResult = GetService().registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return PresentationServices::CancelDialog(a_client, a_dialog);
	}
} // namespace DearModdingUI::HostAPIInternal
