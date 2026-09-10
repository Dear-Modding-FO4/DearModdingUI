#pragma once

#include <DearModdingUI/host/Diagnostics.h>
#include <DearModdingUI/host/Registry.h>
#include <DearModdingUI/host/Status.h>

#include <atomic>
#include <vector>

namespace DearModdingUI::HostInternal
{
	struct Service
	{
		Registry registry;
		std::atomic<DMUI_HostState> state{ DMUI_HOST_STATE_NOT_INITIALIZED };
		std::atomic<DMUI_UnavailableReason> unavailableReason{
			DMUI_UNAVAILABLE_NONE
		};
		std::atomic<DMUI_UnavailableReason> deferredUnavailableReason{
			DMUI_UNAVAILABLE_NONE
		};
		std::atomic<bool> menuVisible{ false };
		std::atomic<DMUI_PageHandle> selectedPage{
			DMUI_INVALID_PAGE_HANDLE
		};
		std::atomic<DMUI_PageHandle> activePage{ DMUI_INVALID_PAGE_HANDLE };
		StatusModel status;
		DiagnosticStore diagnostics;
	};

	struct ClientFontPush
	{
		DMUI_ClientHandle client;
		int depth;
	};

	extern thread_local std::vector<ClientFontPush> g_clientFontPushes;

	[[nodiscard]] Service& GetService() noexcept;
	void SetActivePageState(Service& a_service, DMUI_PageHandle a_page) noexcept;
	void SetMenuVisibleState(Service& a_service, bool a_visible) noexcept;
	[[nodiscard]] DMUI_Result StateResult(DMUI_HostState a_state) noexcept;
	[[nodiscard]] DMUI_Result RegistrationResult(DMUI_HostState a_state) noexcept;
	[[nodiscard]] DMUI_Result ValidateDrawingClient(
		DMUI_ClientHandle a_client) noexcept;
}
