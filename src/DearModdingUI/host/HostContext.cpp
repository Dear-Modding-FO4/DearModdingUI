#include "HostContext.h"

#include <DearModdingUI/settings/HostSettings.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/host/MenuDismissal.h>

namespace DearModdingUI::HostInternal
{
	thread_local std::vector<ClientFontPush> g_clientFontPushes;

	Service& GetService() noexcept
	{
		static Service service;
		return service;
	}

	void SetActivePageState(
		Service& a_service,
		DMUI_PageHandle a_page) noexcept
	{
		const auto previous =
			a_service.activePage.exchange(a_page, std::memory_order_acq_rel);
		a_service.registry.NotifyPageActivity(previous, a_page);
	}

	void SetMenuVisibleState(Service& a_service, bool a_visible) noexcept
	{
		a_service.menuVisible.store(a_visible, std::memory_order_release);
		if (!a_visible)
		{
			ResetMenuEscapeRequest();
			PresentationServices::NotifyMenuClosed();
		}
		HostSettings::NotifyMenuVisible(a_visible);
	}

	DMUI_Result StateResult(DMUI_HostState a_state) noexcept
	{
		switch (a_state)
		{
		case DMUI_HOST_STATE_NOT_INITIALIZED:
			return DMUI_RESULT_HOST_NOT_INITIALIZED;
		case DMUI_HOST_STATE_READY:
			return DMUI_RESULT_OK;
		case DMUI_HOST_STATE_UNAVAILABLE:
			return GetService().unavailableReason.load(std::memory_order_acquire) ==
					DMUI_UNAVAILABLE_HOST_DISABLED ?
				DMUI_RESULT_HOST_DISABLED :
				DMUI_RESULT_BACKEND_FAILED;
		default:
			return DMUI_RESULT_HOST_NOT_READY;
		}
	}

	DMUI_Result ValidateDrawingClient(DMUI_ClientHandle a_client) noexcept
	{
		if (a_client == DMUI_INVALID_CLIENT_HANDLE)
			return DMUI_RESULT_INVALID_ARGUMENT;
		auto& service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state != DMUI_HOST_STATE_READY)
			return StateResult(state);
		return service.registry.ValidateClient(a_client);
	}

	DMUI_Result RegistrationResult(DMUI_HostState a_state) noexcept
	{
		switch (a_state)
		{
		case DMUI_HOST_STATE_WAITING_FOR_PRESENT:
			return DMUI_RESULT_OK;
		case DMUI_HOST_STATE_INITIALIZING:
		case DMUI_HOST_STATE_READY:
			return DMUI_RESULT_REGISTRATION_CLOSED;
		default:
			return StateResult(a_state);
		}
	}
}
