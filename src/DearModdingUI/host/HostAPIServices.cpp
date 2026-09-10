#include "HostAPIEntries.h"
#include "HostContext.h"

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/presentation/PresentationServices.h>

namespace DearModdingUI::HostAPIInternal
{
	using namespace HostInternal;

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

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiDrawImage(DMUI_ClientHandle a_client, DMUI_ImageHandle a_image,
				 const DMUI_ImageDrawOptions *a_options) noexcept
	{
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::DrawImage(a_client, a_image, a_options);
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
		const auto validation = registry.ValidatePage(a_client, a_page, DMUI_PAGE_KIND_OVERLAY);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::ConfigureOverlay(a_client, a_page, a_options);
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

	[[nodiscard]] DMUI_Result DMUI_CALL ApiPostNotification(
		DMUI_ClientHandle a_client, const DMUI_NotificationDescriptor *a_descriptor) noexcept
	{
		const auto clientResult = GetService().registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return PresentationServices::PostNotification(a_client, a_descriptor);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiDrawAnnotatedPlot(DMUI_ClientHandle a_client, const char *a_id,
						 const DMUI_AnnotatedPlotDescriptor *a_descriptor) noexcept
	{
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return PresentationServices::DrawAnnotatedPlot(a_client, a_id, a_descriptor);
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
