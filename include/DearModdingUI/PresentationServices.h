#pragma once

#include <DearModdingUI/API.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace DearModdingUI::PresentationServices
{
	inline constexpr DMUI_HostServices kSupportedServices{
		DMUI_HOST_SERVICE_FRAME_CONTROL |
		DMUI_HOST_SERVICE_EDIT_LIFECYCLE |
		DMUI_HOST_SERVICE_CONTEXTUAL_HOTKEYS |
		DMUI_HOST_SERVICE_IMAGE_RESOURCES |
		DMUI_HOST_SERVICE_MANAGED_OVERLAYS |
		DMUI_HOST_SERVICE_NOTIFICATIONS |
		DMUI_HOST_SERVICE_ANNOTATED_PLOTS |
		DMUI_HOST_SERVICE_DIALOGS |
		DMUI_HOST_SERVICE_PIXEL_IMAGES |
		DMUI_HOST_SERVICE_EXTERNAL_OPEN |
		DMUI_HOST_SERVICE_VIRTUAL_FILE_TARGETS
	};

	class ClientExecutionGuard
	{
	public:
		ClientExecutionGuard(DMUI_ClientHandle a_client, bool a_drawing) noexcept;
		~ClientExecutionGuard() noexcept;

		ClientExecutionGuard(const ClientExecutionGuard&) = delete;
		ClientExecutionGuard(ClientExecutionGuard&&) = delete;
		ClientExecutionGuard& operator=(const ClientExecutionGuard&) = delete;
		ClientExecutionGuard& operator=(ClientExecutionGuard&&) = delete;

	private:
		DMUI_ClientHandle m_previousClient;
		bool m_previousDrawing;
	};

	void BindRenderer(ID3D11Device* a_device) noexcept;
	void SetDevice(ID3D11Device* a_device) noexcept;
	void InvalidateDevice() noexcept;
	void BeginFrame() noexcept;
	void CompleteRenderSubmission() noexcept;
	void DiscardFrame() noexcept;

	[[nodiscard]] bool IsActiveClient(
		DMUI_ClientHandle a_client,
		bool a_drawingRequired) noexcept;
	[[nodiscard]] uint64_t DeviceGeneration() noexcept;

	[[nodiscard]] DMUI_Result ImportD3D11Image(
		DMUI_ClientHandle a_client,
		const DMUI_D3D11ImageDescriptor* a_descriptor,
		DMUI_ImageHandle* a_image) noexcept;
	[[nodiscard]] DMUI_Result CreateImage(
		DMUI_ClientHandle a_client,
		const DMUI_ImageDescriptor* a_descriptor,
		DMUI_ImageHandle* a_image) noexcept;
	[[nodiscard]] DMUI_Result UpdateImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image,
		const DMUI_ImageDescriptor* a_descriptor) noexcept;
	[[nodiscard]] DMUI_Result DrawImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image,
		const DMUI_ImageDrawOptions* a_options) noexcept;
	[[nodiscard]] DMUI_Result ReleaseImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image) noexcept;
	[[nodiscard]] DMUI_Result QueryImage(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image,
		DMUI_ImageInfo* a_info) noexcept;
	[[nodiscard]] size_t ImageSlotCount() noexcept;
	[[nodiscard]] ID3D11ShaderResourceView* RetainImageViewForTests(
		DMUI_ClientHandle a_client,
		DMUI_ImageHandle a_image) noexcept;

	[[nodiscard]] DMUI_Result ConfigureOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		const DMUI_ManagedOverlayOptions* a_options) noexcept;
	[[nodiscard]] DMUI_Result QueryOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		DMUI_ManagedOverlayPlacement* a_placement) noexcept;
	enum class ManagedOverlayBeginResult
	{
		kNotConfigured,
		kHidden,
		kVisible
	};

	[[nodiscard]] ManagedOverlayBeginResult BeginManagedOverlay(
		DMUI_ClientHandle a_client,
		DMUI_PageHandle a_page,
		std::string_view a_label,
		bool a_menuVisible) noexcept;
	void EndManagedOverlay() noexcept;

	[[nodiscard]] DMUI_Result PostNotification(
		DMUI_ClientHandle a_client,
		const DMUI_NotificationDescriptor* a_descriptor) noexcept;
	void DrawNotification() noexcept;

	[[nodiscard]] DMUI_Result DrawAnnotatedPlot(
		DMUI_ClientHandle a_client,
		const char* a_id,
		const DMUI_AnnotatedPlotDescriptor* a_descriptor) noexcept;

	[[nodiscard]] DMUI_Result RequestDialog(
		DMUI_ClientHandle a_client,
		const DMUI_DialogDescriptor* a_descriptor,
		DMUI_DialogHandle* a_dialog,
		bool a_menuVisible) noexcept;
	[[nodiscard]] DMUI_Result PollDialogEvent(
		DMUI_ClientHandle a_client,
		DMUI_DialogHandle a_dialog,
		DMUI_DialogEvent* a_event,
		char* a_textBuffer,
		uint32_t a_textCapacity) noexcept;
	[[nodiscard]] DMUI_Result ResolveDialogSubmission(
		DMUI_ClientHandle a_client,
		DMUI_DialogHandle a_dialog,
		uint64_t a_submissionId,
		uint32_t a_accepted,
		const char* a_error) noexcept;
	[[nodiscard]] DMUI_Result CancelDialog(
		DMUI_ClientHandle a_client,
		DMUI_DialogHandle a_dialog) noexcept;
	[[nodiscard]] DMUI_Result SubmitDialog(
		DMUI_DialogHandle a_dialog) noexcept;
	void DrawDialog(bool a_menuVisible) noexcept;
	void NotifyMenuClosed() noexcept;

	[[nodiscard]] bool HasFrameDemand() noexcept;
	[[nodiscard]] bool HasActiveDialog() noexcept;
	[[nodiscard]] uint32_t ActiveDialogPopupId() noexcept;
}
