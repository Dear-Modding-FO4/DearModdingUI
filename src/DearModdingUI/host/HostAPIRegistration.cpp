#include "HostAPIEntries.h"
#include "HostContext.h"

#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/SwapChainAttachment.h>
#include <DearModdingUI/host/UIAdapter.h>
#include <Platform/rendering/PlatformImGui.h>

namespace DearModdingUI::HostAPIInternal
{
	using namespace HostInternal;

	[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterClient(const DMUI_ClientDescriptor *a_descriptor,
														  DMUI_ClientHandle *a_client) noexcept
	{
		if (!a_descriptor || !a_client)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_client = DMUI_INVALID_CLIENT_HANDLE;
		auto &service = GetService();
		const auto result = RegistrationResult(service.state.load(std::memory_order_acquire));
		if (result != DMUI_RESULT_OK)
			return result;
		return service.registry.RegisterClient(a_descriptor, a_client);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterPage(DMUI_ClientHandle a_client,
														const DMUI_PageDescriptor *a_descriptor,
														DMUI_PageHandle *a_page) noexcept
	{
		if (!a_descriptor || !a_page || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_page = DMUI_INVALID_PAGE_HANDLE;
		auto &service = GetService();
		const auto result = RegistrationResult(service.state.load(std::memory_order_acquire));
		if (result != DMUI_RESULT_OK)
			return result;
		return service.registry.RegisterPage(a_client, a_descriptor, a_page);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterCategory(
		DMUI_ClientHandle a_client, const DMUI_CategoryDescriptor *a_descriptor) noexcept
	{
		if (!a_descriptor || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto &service = GetService();
		const auto result = RegistrationResult(service.state.load(std::memory_order_acquire));
		if (result != DMUI_RESULT_OK)
			return result;
		return service.registry.RegisterCategory(a_client, a_descriptor);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterAction(DMUI_ClientHandle a_client,
														  const DMUI_ActionDescriptor *a_descriptor,
														  DMUI_ActionHandle *a_action) noexcept
	{
		if (!a_descriptor || !a_action || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_action = DMUI_INVALID_ACTION_HANDLE;
		auto &service = GetService();
		const auto result = RegistrationResult(service.state.load(std::memory_order_acquire));
		if (result != DMUI_RESULT_OK)
			return result;
		return service.registry.RegisterAction(a_client, a_descriptor, a_action);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterFrameObserver(
		DMUI_ClientHandle a_client, const DMUI_FrameObserverDescriptor *a_descriptor,
		DMUI_FrameObserverHandle *a_observer) noexcept
	{
		if (!a_descriptor || !a_observer || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_observer = DMUI_INVALID_FRAME_OBSERVER_HANDLE;
		auto &service = GetService();
		const auto result = RegistrationResult(service.state.load(std::memory_order_acquire));
		if (result != DMUI_RESULT_OK)
			return result;
		return service.registry.RegisterFrameObserver(a_client, a_descriptor, a_observer);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterPageActivityObserver(
		DMUI_ClientHandle a_client, const DMUI_PageActivityObserverDescriptor *a_descriptor,
		DMUI_PageActivityObserverHandle *a_observer) noexcept
	{
		if (!a_descriptor || !a_observer || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_observer = DMUI_INVALID_PAGE_ACTIVITY_OBSERVER_HANDLE;
		auto &service = GetService();
		const auto result = RegistrationResult(service.state.load(std::memory_order_acquire));
		if (result != DMUI_RESULT_OK)
			return result;
		return service.registry.RegisterPageActivityObserver(a_client, a_descriptor, a_observer);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiRegisterHotkeyAction(
		DMUI_ClientHandle a_client, const DMUI_HotkeyActionDescriptor *a_descriptor,
		DMUI_HotkeyActionHandle *a_action) noexcept
	{
		if (!a_descriptor || !a_action || a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_action = DMUI_INVALID_HOTKEY_ACTION_HANDLE;
		auto &service = GetService();
		const auto result = RegistrationResult(service.state.load(std::memory_order_acquire));
		if (result != DMUI_RESULT_OK)
			return result;
		const auto clientResult = service.registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return Hotkeys::Register(a_client, a_descriptor, a_action);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiQueryHotkeyBinding(DMUI_ClientHandle a_client, DMUI_HotkeyActionHandle a_action,
						  DMUI_HotkeyBindingInfo *a_binding) noexcept
	{
		const auto clientResult = GetService().registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return Hotkeys::Query(a_client, a_action, a_binding);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL
	ApiUnregisterHotkeyAction(DMUI_ClientHandle a_client, DMUI_HotkeyActionHandle a_action) noexcept
	{
		const auto clientResult = GetService().registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return Hotkeys::Unregister(a_client, a_action);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryState(DMUI_HostStateInfo *a_state) noexcept
	{
		if (!a_state)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_state->structSize < sizeof(DMUI_HostStateInfo))
			return DMUI_RESULT_STRUCT_TOO_SMALL;

		auto &service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		const auto clientCount = service.registry.ClientCount();
		const auto pageCount = service.registry.PageCount();
		const auto demandedCount = service.registry.DemandedOverlayCount();
		if (clientCount > (std::numeric_limits<uint32_t>::max)() ||
			pageCount > (std::numeric_limits<uint32_t>::max)() ||
			demandedCount > (std::numeric_limits<uint32_t>::max)())
			return DMUI_RESULT_RESOURCE_EXHAUSTED;

		a_state->state = state;
		a_state->unavailableReason = service.unavailableReason.load(std::memory_order_acquire);
		a_state->registrationOpen =
			state == DMUI_HOST_STATE_WAITING_FOR_PRESENT && service.registry.IsOpen() ? 1u : 0u;
		a_state->clientCount = static_cast<uint32_t>(clientCount);
		a_state->pageCount = static_cast<uint32_t>(pageCount);
		a_state->demandedOverlayCount = static_cast<uint32_t>(demandedCount);
		return StateResult(state);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiRequestFrame(DMUI_ClientHandle a_client,
														DMUI_PageHandle a_page) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE || a_page == DMUI_INVALID_PAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto &service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state == DMUI_HOST_STATE_NOT_INITIALIZED || state == DMUI_HOST_STATE_UNAVAILABLE)
			return StateResult(state);
		return service.registry.RequestFrame(a_client, a_page);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiReleaseFrame(DMUI_ClientHandle a_client,
														DMUI_PageHandle a_page) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE || a_page == DMUI_INVALID_PAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto &service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state == DMUI_HOST_STATE_NOT_INITIALIZED || state == DMUI_HOST_STATE_UNAVAILABLE)
			return StateResult(state);
		return service.registry.ReleaseFrame(a_client, a_page);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiIsMenuVisible(uint32_t *a_visible) noexcept
	{
		if (!a_visible)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto state = GetService().state.load(std::memory_order_acquire);
		*a_visible = state == DMUI_HOST_STATE_READY && IsMenuVisible() ? 1u : 0u;
		return StateResult(state);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiSelectPage(DMUI_ClientHandle a_client,
													  DMUI_PageHandle a_page) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE || a_page == DMUI_INVALID_PAGE_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto &service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state != DMUI_HOST_STATE_READY)
			return StateResult(state);
		const auto valid = service.registry.ValidatePage(a_client, a_page, DMUI_PAGE_KIND_SETTINGS);
		if (valid != DMUI_RESULT_OK)
			return valid;
		service.selectedPage.store(a_page, std::memory_order_release);
		SetMenuVisibleState(service, true);
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiAttachSwapChain(DMUI_ClientHandle a_client,
														   void *a_nativeSwapChain) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE || !a_nativeSwapChain)
			return DMUI_RESULT_INVALID_ARGUMENT;

		auto &service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state == DMUI_HOST_STATE_INITIALIZING)
			return DMUI_RESULT_RENDERER_BUSY;
		if (state != DMUI_HOST_STATE_WAITING_FOR_PRESENT && state != DMUI_HOST_STATE_READY)
			return StateResult(state);

		const auto clientResult = service.registry.ValidateSwapChainClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return SwapChainAttachmentResult(Addictol::PlatformImgui::AttachSwapChain(
			static_cast<IDXGISwapChain *>(a_nativeSwapChain)));
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiSetStatus(DMUI_ClientHandle a_client,
													 DMUI_StatusSeverity a_severity,
													 const char *a_message) noexcept
	{
		auto &service = GetService();
		std::string owner;
		const auto validation = ValidateStatusRequest(
			service.registry.CopyClientDisplayName(a_client, owner), a_severity, a_message);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return service.status.SetClient(a_client, owner, a_severity, a_message);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiReportDiagnostic(
		DMUI_ClientHandle a_client, const DMUI_DiagnosticDescriptor *a_diagnostic) noexcept
	{
		const auto validation = ValidateDiagnosticArguments(a_client, a_diagnostic);
		if (validation != DMUI_RESULT_OK)
			return validation;

		auto &service = GetService();
		const auto clientResult = service.registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return service.diagnostics.Report(a_client, *a_diagnostic);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryVideoMemory(DMUI_ClientHandle a_client,
															uint64_t *a_used,
															uint64_t *a_budget) noexcept
	{
		if (!a_used || !a_budget)
			return DMUI_RESULT_INVALID_ARGUMENT;
		*a_used = 0;
		*a_budget = 0;
		const auto validation = ValidateDrawingClient(a_client);
		if (validation != DMUI_RESULT_OK)
			return validation;
		return Addictol::PlatformImgui::QueryVideoMemory(*a_used, *a_budget)
				   ? DMUI_RESULT_OK
				   : DMUI_RESULT_BACKEND_FAILED;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryServices(DMUI_HostServicesInfo *a_services) noexcept
	{
		if (!a_services)
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_services->structSize < DMUI_HOST_SERVICES_INFO_0_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		a_services->supportedServices = PresentationServices::kSupportedServices;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiQueryUIAPI(uint32_t a_requestedUIAbi,
													  uint32_t a_minimumRevision,
													  uint32_t a_minimumTableSize,
													  DMUI_UIAPIInfo *a_info) noexcept
	{
		return UI::Query(a_requestedUIAbi, a_minimumRevision, a_minimumTableSize, a_info);
	}

	[[nodiscard]] DMUI_Result DMUI_CALL ApiSetHotkeyActionEnabled(DMUI_ClientHandle a_client,
																  DMUI_HotkeyActionHandle a_action,
																  uint32_t a_enabled) noexcept
	{
		const auto clientResult = GetService().registry.ValidateClient(a_client);
		if (clientResult != DMUI_RESULT_OK)
			return clientResult;
		return Hotkeys::SetEnabled(a_client, a_action, a_enabled != 0);
	}
} // namespace DearModdingUI::HostAPIInternal
