#pragma once

#include <DearModdingUI/API.h>
#include <DearModdingUI/host/Diagnostics.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/Registry.h>
#include <DearModdingUI/host/Status.h>

#include <optional>
#include <string_view>

#ifndef DMUI_VERSION
#error "DMUI_VERSION must be supplied by the host build."
#endif

namespace DearModdingUI
{
	inline constexpr std::string_view kHostDisplayName{ "Evil Modding" };
	inline constexpr std::string_view kHostVersion{ DMUI_VERSION };

	// Older minors of this major see a prefix of the current tables.
	[[nodiscard]] constexpr bool ServesAbiVersion(uint32_t a_requested) noexcept
	{
		return DMUI_ABI_VERSION_MAJOR(a_requested) == DMUI_ABI_MAJOR &&
			DMUI_ABI_VERSION_MINOR(a_requested) <= DMUI_ABI_MINOR;
	}

	[[nodiscard]] const DMUI_HostAPI& HostAPI() noexcept;

	void Initialize() noexcept;
	void SetBackendUnavailable(DMUI_UnavailableReason a_reason) noexcept;
	void DeferBackendUnavailable(DMUI_UnavailableReason a_reason) noexcept;
	[[nodiscard]] bool BeginBackendInitialization() noexcept;
	void CompleteBackendInitialization(void* a_imguiContext) noexcept;
	void FailBackendInitialization() noexcept;
	[[nodiscard]] bool NeedsFrame() noexcept;
	[[nodiscard]] bool HasSettingsPages() noexcept;
	[[nodiscard]] bool IsMenuVisible() noexcept;
	[[nodiscard]] HostInputMode CurrentInputMode() noexcept;
	[[nodiscard]] HotkeyContextState CurrentHotkeyContext(bool a_gameplaySafe) noexcept;
	void EndOverlayFocus(DMUI_OverlayFocusEndReason a_reason) noexcept;
	[[nodiscard]] DMUI_Result SetMenuVisible(bool a_visible) noexcept;
	void CloseMenu() noexcept;
	[[nodiscard]] DMUI_PageHandle SelectedPage() noexcept;
	void ClearPageSelection(DMUI_PageHandle a_page) noexcept;
	void SetActivePage(DMUI_PageHandle a_page) noexcept;
	[[nodiscard]] bool DrawPage(DMUI_PageHandle a_page) noexcept;
	[[nodiscard]] bool PageFailed(DMUI_PageHandle a_page) noexcept;
	[[nodiscard]] bool InvokeAction(DMUI_ActionHandle a_action) noexcept;
	[[nodiscard]] bool ActionFailed(DMUI_ActionHandle a_action) noexcept;
	void ObserveFrame() noexcept;
	void DrawDemandedOverlays() noexcept;
	[[nodiscard]] const std::vector<RegisteredClient>& RegisteredClients() noexcept;
	[[nodiscard]] const std::vector<RegisteredPage>& OrderedPages() noexcept;
	[[nodiscard]] const std::vector<RegisteredAction>& OrderedActions() noexcept;
	[[nodiscard]] const NavigationModel& Navigation() noexcept;
	[[nodiscard]] DMUI_Result SetHostStatus(
		DMUI_StatusSeverity a_severity,
		std::string_view a_message) noexcept;
	[[nodiscard]] std::optional<StatusMessage> CurrentStatus() noexcept;
	[[nodiscard]] std::vector<ClientStatus> CurrentClientStatuses() noexcept;
	[[nodiscard]] std::vector<ClientDiagnosticSnapshot>
		CurrentClientDiagnostics() noexcept;
	[[nodiscard]] bool DismissStatus(uint64_t a_generation) noexcept;
}
