#pragma once

#include <DearModdingUI/navigation/NavigationPresentation.h>
#include <DearModdingUI/navigation/Sidebar.h>
#include <DearModdingUI/host/Status.h>

#include <optional>
#include <span>

namespace DearModdingUI
{
	struct SidebarViewState : SidebarBrowsingState
	{
#if defined(DMUI_PREVIEW)
		std::optional<std::vector<std::string>> previewExpandedClients;
#endif
	};

	struct SidebarDrawResult
	{
		std::optional<NavigationRequest> request;
		bool openPalette{ false };
	};

	struct SidebarNavigationIntent
	{
		std::optional<NavigationRequest> request;

		void Offer(NavigationRequest a_request) noexcept
		{
			if (!request)
				request = a_request;
		}
	};

	enum class SidebarClientRowKind : uint32_t
	{
		kTree,
		kList,
		kRail
	};

	struct SidebarViewContext
	{
		const NavigationModel& model;
		std::span<const ClientStatus> statuses;
		const NavigationPresentation& presentation;
		const ClientSelectionState& selection;
		SidebarViewState& browsing;
		SidebarNavigationIntent& intent;
	};

	void DrawPresentedSidebarClients(
		const SidebarViewContext& a_context,
		SidebarClientRowKind a_kind) noexcept;
	void DrawSidebarPageList(
		const SidebarViewContext& a_context,
		const NavigationClient& a_client,
		bool a_indented = true) noexcept;
	[[nodiscard]] SidebarDrawResult DrawSidebar(
		const NavigationModel& a_model,
		std::span<const ClientStatus> a_statuses,
		const ClientSelectionState& a_selection,
		SidebarLayoutKind a_layout,
		NavigationPresentationKind a_presentationKind,
		NavigationPresentationState& a_presentationState,
		SidebarViewState& a_state) noexcept;
}
