#define DMUI_HOST_EXPORTS
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/host/Host.h>
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/ImGuiRecovery.h>
#include <DearModdingUI/presentation/PresentationServices.h>
#include <DearModdingUI/controls/SettingsTable.h>
#include "HostAPIEntries.h"
#include "HostContext.h"

#include <REX/REX.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <string_view>

#ifndef IMGUI_HAS_DOCK
#error "DearModdingUI requires the pinned Dear ImGui docking build"
#endif

namespace DearModdingUI
{
	using namespace std::literals;

	namespace
	{
		using namespace HostAPIInternal;
		using namespace HostInternal;

		struct ClientCallbackIdentity
		{
			const char* kind{ "unknown" };
			uint64_t handle{ 0 };
			std::string_view id{ "<unknown>" };
			std::string_view displayName{ "<unknown>" };
			std::string_view clientId{ "<unknown>" };
			std::string_view clientDisplayName{ "<unknown>" };
		};

		// Pages and actions draw inside the frame, so isolate the host's frame state.
		void LogImGuiRecovery(
			const ClientCallbackIdentity& a_identity,
			const ImGuiRecoveryResult& a_recovery) noexcept
		{
			if (!a_recovery.Repaired())
				return;

			const auto& before = a_recovery.before;
			const auto& after = a_recovery.after;
			REX::ERROR(
				"DearModdingUI: {} callback {} [id \"{}\" (\"{}\"), "
				"client \"{}\" (\"{}\")] required ImGui recovery "
				"(windows {}->{}, tables {}->{}, IDs {}->{}, trees {}->{}, "
				"colors {}->{}, style vars {}->{}, fonts {}->{}, focus scopes {}->{}, "
				"groups {}->{}, item flags {}->{}, popups {}->{}, disabled {}->{})"sv,
				a_identity.kind,
				a_identity.handle,
				a_identity.id,
				a_identity.displayName,
				a_identity.clientId,
				a_identity.clientDisplayName,
				before.windows,
				after.windows,
				before.tables,
				after.tables,
				before.ids,
				after.ids,
				before.trees,
				after.trees,
				before.colors,
				after.colors,
				before.styleVariables,
				after.styleVariables,
				before.fonts,
				after.fonts,
				before.focusScopes,
				after.focusScopes,
				before.groups,
				after.groups,
				before.itemFlags,
				after.itemFlags,
				before.popups,
				after.popups,
				before.disabled,
				after.disabled);
		}

		template <class InvokeCallback, class DisableCallback>
		[[nodiscard]] bool InvokeClientCallback(
			const ClientCallbackIdentity& a_identity,
			InvokeCallback&& a_invoke,
			DisableCallback&& a_disable) noexcept
		{
			auto recovery = ImGuiRecoverySnapshot::Capture();
			if (!recovery)
			{
				g_clientFontPushes.clear();
				a_disable();
				REX::ERROR(
					"DearModdingUI: {} callback {} [id \"{}\" (\"{}\"), "
					"client \"{}\" (\"{}\")] could not be isolated and was disabled"sv,
					a_identity.kind,
					a_identity.handle,
					a_identity.id,
					a_identity.displayName,
					a_identity.clientId,
					a_identity.clientDisplayName);
				return false;
			}
			const auto result = a_invoke();
			if (result != DMUI_RESULT_OK)
			{
				const auto recovered = recovery->RecoverFailure();
				LogImGuiRecovery(a_identity, recovered);
				g_clientFontPushes.clear();
				REX::ERROR(
					"DearModdingUI: {} callback {} [id \"{}\" (\"{}\"), "
					"client \"{}\" (\"{}\")] failed with {} and was disabled"sv,
					a_identity.kind,
					a_identity.handle,
					a_identity.id,
					a_identity.displayName,
					a_identity.clientId,
					a_identity.clientDisplayName,
					DMUI_ResultToString(result));
				return false;
			}
			const auto recovered = recovery->RecoverAfterCallback();
			LogImGuiRecovery(a_identity, recovered);
			g_clientFontPushes.clear();
			return result == DMUI_RESULT_OK;
		}

		// Frame observers run after Present, outside the frame, where there is nothing to isolate.
		template <class InvokeCallback>
		[[nodiscard]] bool InvokeNonDrawingClientCallback(
			const ClientCallbackIdentity& a_identity,
			InvokeCallback&& a_invoke) noexcept
		{
			const auto result = a_invoke();
			if (result == DMUI_RESULT_CALLBACK_FAILED)
			{
				REX::ERROR(
					"DearModdingUI: {} callback {} [id \"{}\" (\"{}\"), "
					"client \"{}\" (\"{}\")] failed and was disabled"sv,
					a_identity.kind,
					a_identity.handle,
					a_identity.id,
					a_identity.displayName,
					a_identity.clientId,
					a_identity.clientDisplayName);
				return false;
			}
			return result == DMUI_RESULT_OK;
		}
	}

	const DMUI_HostAPI& HostAPI() noexcept
	{
		// /EHsc SEH would bypass registry lock destructors, so entry points stay direct.
		static const DMUI_HostAPI api{
			sizeof(DMUI_HostAPI),
			DMUI_HOST_ABI_CURRENT,
			DMUI_API_VERSION_CURRENT,
			&ApiRegisterClient,
			&ApiRegisterPage,
			&ApiQueryState,
			&ApiRequestFrame,
			&ApiReleaseFrame,
			&ApiIsMenuVisible,
			&ApiSelectPage,
			&ApiAttachSwapChain,
			&ApiRegisterAction,
			&ApiSetStatus,
			&ApiGetThemeColors,
			&ApiPushFont,
			&ApiPopFont,
			&ApiDrawSectionHeader,
			&ApiDrawSearchInput,
			&ApiDrawCollapsingSectionHeader,
			&ApiDrawSettingsActionButton,
			&ApiSettingsActionButtonWidth,
			&ApiSettingsActionButtonExtent,
			&ApiRegisterFrameObserver,
			&ApiQueryVideoMemory,
			&ApiDrawBulletText,
			&ApiRegisterHotkeyAction,
			&ApiQueryHotkeyBinding,
			&ApiUnregisterHotkeyAction,
			&ApiBeginSettingsTable,
			&ApiBeginSettingsRow,
			&ApiEndSettingsRow,
			&ApiEndSettingsTable,
			&ApiBeginSettingsRowEx,
			&ApiRegisterPageActivityObserver,
			&ApiDrawLinkRow,
			&ApiDrawFaq,
			&ApiReportDiagnostic,
			&ApiQueryServices,
			&ApiSetHotkeyActionEnabled,
			&ApiImportD3D11Image,
			&ApiDrawImage,
			&ApiReleaseImage,
			&ApiQueryImage,
			&ApiConfigureOverlay,
			&ApiQueryOverlay,
			&ApiPostNotification,
			&ApiDrawAnnotatedPlot,
			&ApiRequestDialog,
			&ApiPollDialogEvent,
			&ApiResolveDialogSubmission,
			&ApiCancelDialog,
			&ApiCreateImage,
			&ApiUpdateImage,
			&ApiRegisterCategory,
			&ApiOpenExternal,
			&ApiQueryUIAPI
		};
		return api;
	}

	void Initialize() noexcept
	{
		auto& service = GetService();
		auto expected = DMUI_HOST_STATE_NOT_INITIALIZED;
		service.state.compare_exchange_strong(
			expected,
			DMUI_HOST_STATE_WAITING_FOR_PRESENT,
			std::memory_order_acq_rel);
	}

	void SetBackendUnavailable(DMUI_UnavailableReason a_reason) noexcept
	{
		auto& service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state == DMUI_HOST_STATE_UNAVAILABLE)
			return;
		service.unavailableReason.store(a_reason, std::memory_order_release);
		service.state.store(DMUI_HOST_STATE_UNAVAILABLE, std::memory_order_release);
		SetMenuVisibleState(service, false);
	}

	void DeferBackendUnavailable(DMUI_UnavailableReason a_reason) noexcept
	{
		GetService().deferredUnavailableReason.store(a_reason, std::memory_order_release);
	}

	bool BeginBackendInitialization() noexcept
	{
		auto& service = GetService();
		auto expected = DMUI_HOST_STATE_WAITING_FOR_PRESENT;
		if (!service.state.compare_exchange_strong(
				expected,
				DMUI_HOST_STATE_INITIALIZING,
				std::memory_order_acq_rel))
			return expected == DMUI_HOST_STATE_INITIALIZING ||
				expected == DMUI_HOST_STATE_READY;
		if (!service.registry.Freeze())
		{
			FailBackendInitialization();
			return false;
		}
		if (service.deferredUnavailableReason.load(std::memory_order_acquire) !=
			DMUI_UNAVAILABLE_NONE)
		{
			FailBackendInitialization();
			return false;
		}
		return true;
	}

	void CompleteBackendInitialization(void*) noexcept
	{
		auto& service = GetService();
		auto expected = DMUI_HOST_STATE_INITIALIZING;
		if (!service.state.compare_exchange_strong(
				expected,
				DMUI_HOST_STATE_READY,
				std::memory_order_acq_rel))
			return;

		const DMUI_HostReadyInfo info{
			sizeof(DMUI_HostReadyInfo),
			DMUI_API_VERSION_CURRENT
		};
		service.registry.NotifyReady(info);
	}

	void FailBackendInitialization() noexcept
	{
		auto& service = GetService();
		if (service.state.exchange(
				DMUI_HOST_STATE_UNAVAILABLE,
				std::memory_order_acq_rel) == DMUI_HOST_STATE_UNAVAILABLE)
			return;
		service.unavailableReason.store(
			DMUI_UNAVAILABLE_BACKEND_FAILED,
			std::memory_order_release);
		SetMenuVisibleState(service, false);
		if (service.registry.IsOpen())
			(void)service.registry.Freeze();
		service.registry.NotifyUnavailable(DMUI_UNAVAILABLE_BACKEND_FAILED);
	}

	bool NeedsFrame() noexcept
	{
		auto& service = GetService();
		return service.menuVisible.load(std::memory_order_acquire) ||
			service.registry.DemandedOverlayCount() != 0 ||
			PresentationServices::HasFrameDemand();
	}

	bool HasSettingsPages() noexcept
	{
		return GetService().registry.HasSettingsPages();
	}

	bool IsMenuVisible() noexcept
	{
		return GetService().menuVisible.load(std::memory_order_acquire);
	}

	DMUI_Result SetMenuVisible(bool a_visible) noexcept
	{
		auto& service = GetService();
		const auto state = service.state.load(std::memory_order_acquire);
		if (state != DMUI_HOST_STATE_READY)
			return StateResult(state);
		SetMenuVisibleState(service, a_visible);
		return DMUI_RESULT_OK;
	}

	void CloseMenu() noexcept
	{
		SetMenuVisibleState(GetService(), false);
	}

	DMUI_PageHandle SelectedPage() noexcept
	{
		return GetService().selectedPage.load(std::memory_order_acquire);
	}

	void ClearPageSelection(DMUI_PageHandle a_page) noexcept
	{
		auto& selected = GetService().selectedPage;
		selected.compare_exchange_strong(
			a_page,
			DMUI_INVALID_PAGE_HANDLE,
			std::memory_order_acq_rel);
	}

	void SetActivePage(DMUI_PageHandle a_page) noexcept
	{
		SetActivePageState(GetService(), a_page);
	}

	bool DrawPage(DMUI_PageHandle a_page) noexcept
	{
		auto& service = GetService();
		const auto& pages = service.registry.OrderedPages();
		const auto page = std::ranges::find(
			pages,
			a_page,
			&RegisteredPage::handle);
		const SettingsTable::ClientCallbackGuard settingsTableGuard{
			page != pages.end() &&
					page->kind == DMUI_PAGE_KIND_SETTINGS ?
				page->client :
				DMUI_INVALID_CLIENT_HANDLE
		};
		const RenderExecution::ClientGuard executionGuard{
			page != pages.end() ? page->client : DMUI_INVALID_CLIENT_HANDLE,
			true
		};
		const auto identity = page != pages.end() ?
			ClientCallbackIdentity{
				"page",
				a_page,
				page->id,
				page->displayName,
				page->clientId,
				page->clientDisplayName
			} :
			ClientCallbackIdentity{ "page", a_page };
		return InvokeClientCallback(
			identity,
			[&]() noexcept {
				return service.registry.InvokePage(a_page);
			},
			[&]() noexcept {
				service.registry.MarkPageFailed(a_page);
			});
	}

	bool PageFailed(DMUI_PageHandle a_page) noexcept
	{
		return GetService().registry.PageFailed(a_page);
	}

	bool InvokeAction(DMUI_ActionHandle a_action) noexcept
	{
		auto& service = GetService();
		const auto& actions = service.registry.OrderedActions();
		const auto action = std::ranges::find(
			actions,
			a_action,
			&RegisteredAction::handle);
		const auto identity = action != actions.end() ?
			ClientCallbackIdentity{
				"action",
				a_action,
				action->id,
				action->displayLabel,
				action->clientId,
				action->clientDisplayName
			} :
			ClientCallbackIdentity{ "action", a_action };
		const RenderExecution::ClientGuard executionGuard{
			action != actions.end() ?
			action->client :
			DMUI_INVALID_CLIENT_HANDLE,
			false
		};
		return InvokeClientCallback(
			identity,
			[&]() noexcept {
				return service.registry.InvokeAction(a_action);
			},
			[&]() noexcept {
				service.registry.MarkActionFailed(a_action);
			});
	}

	bool ActionFailed(DMUI_ActionHandle a_action) noexcept
	{
		return GetService().registry.ActionFailed(a_action);
	}

	void ObserveFrame() noexcept
	{
		auto& service = GetService();
		auto& registry = service.registry;
		if (!service.menuVisible.load(std::memory_order_acquire))
			SetActivePageState(service, DMUI_INVALID_PAGE_HANDLE);
		if (service.state.load(std::memory_order_acquire) != DMUI_HOST_STATE_READY)
			return;
		Hotkeys::DispatchQueued();
		if (!registry.HasActiveFrameObservers())
			return;
		for (const auto& observer : registry.OrderedFrameObservers())
		{
			if (observer.callbackFailed)
				continue;
			const auto* client = registry.Navigation().FindClient(observer.client);
			const auto identity = client ?
				ClientCallbackIdentity{
					"frame observer",
					observer.handle,
					"<unnamed>",
					"Frame observer",
					client->id,
					client->displayName
				} :
				ClientCallbackIdentity{
					"frame observer",
					observer.handle,
					"<unnamed>",
					"Frame observer"
				};
			(void)InvokeNonDrawingClientCallback(
				identity,
				[&]() noexcept {
					const RenderExecution::ClientGuard executionGuard{
						observer.client,
						false
					};
					return registry.InvokeFrameObserver(observer.handle);
				});
		}
	}

	void DrawDemandedOverlays() noexcept
	{
		auto& registry = GetService().registry;
		for (const auto& page : registry.OrderedPages())
		{
			if (page.kind == DMUI_PAGE_KIND_OVERLAY &&
				registry.IsFrameDemanded(page.handle))
			{
				const auto managed =
					PresentationServices::BeginManagedOverlay(
						page.client,
						page.handle,
						page.imguiLabel,
						IsMenuVisible());
				if (managed !=
					PresentationServices::ManagedOverlayBeginResult::kNotConfigured)
				{
					if (managed ==
						PresentationServices::ManagedOverlayBeginResult::kVisible)
						(void)DrawPage(page.handle);
					PresentationServices::EndManagedOverlay();
				}
				else
					(void)DrawPage(page.handle);
			}
		}
	}

	const std::vector<RegisteredClient>& RegisteredClients() noexcept
	{
		return GetService().registry.RegisteredClients();
	}

	const std::vector<RegisteredPage>& OrderedPages() noexcept
	{
		return GetService().registry.OrderedPages();
	}

	const std::vector<RegisteredAction>& OrderedActions() noexcept
	{
		return GetService().registry.OrderedActions();
	}

	const NavigationModel& Navigation() noexcept
	{
		return GetService().registry.Navigation();
	}

	DMUI_Result SetHostStatus(
		DMUI_StatusSeverity a_severity,
		std::string_view a_message) noexcept
	{
		return GetService().status.Set(
			StatusOwnerKind::kHost,
			kHostDisplayName,
			a_severity,
			a_message);
	}

	std::optional<StatusMessage> CurrentStatus() noexcept
	{
		return GetService().status.Snapshot();
	}

	std::vector<ClientStatus> CurrentClientStatuses() noexcept
	{
		return GetService().status.SnapshotClientStatuses();
	}

	std::vector<ClientDiagnosticSnapshot> CurrentClientDiagnostics() noexcept
	{
		return GetService().diagnostics.Snapshots();
	}

	bool DismissStatus(uint64_t a_generation) noexcept
	{
		return GetService().status.Dismiss(a_generation);
	}

}

DMUI_EXPORT const DMUI_HostAPI* DMUI_CALL DMUI_GetAPI(
	uint32_t a_requestedHostAbi) noexcept
{
	return a_requestedHostAbi == DMUI_HOST_ABI_CURRENT ?
		&DearModdingUI::HostAPI() :
		nullptr;
}
