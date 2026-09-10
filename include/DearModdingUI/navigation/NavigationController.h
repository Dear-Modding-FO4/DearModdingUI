#pragma once

#include <DearModdingUI/navigation/Navigation.h>

#include <cstdint>

namespace DearModdingUI
{
	enum class NavigationRequestKind : uint32_t
	{
		Page,
		Client,
		Host
	};

	struct NavigationRequest
	{
		NavigationRequestKind kind{ NavigationRequestKind::Page };
		DMUI_PageHandle page{ DMUI_INVALID_PAGE_HANDLE };
		DMUI_ClientHandle client{ DMUI_INVALID_CLIENT_HANDLE };
		HostPageKind hostPage{ HostPageKind::kHome };

		[[nodiscard]] static constexpr NavigationRequest Page(
			DMUI_PageHandle a_page) noexcept
		{
			return { NavigationRequestKind::Page, a_page };
		}

		[[nodiscard]] static constexpr NavigationRequest Client(
			DMUI_ClientHandle a_client) noexcept
		{
			return {
				NavigationRequestKind::Client,
				DMUI_INVALID_PAGE_HANDLE,
				a_client
			};
		}

		[[nodiscard]] static constexpr NavigationRequest Host(
			HostPageKind a_page) noexcept
		{
			return {
				NavigationRequestKind::Host,
				DMUI_INVALID_PAGE_HANDLE,
				DMUI_INVALID_CLIENT_HANDLE,
				a_page
			};
		}
	};

	struct NavigationResult
	{
		bool accepted{ false };
		bool selectionChanged{ false };
		bool revealSelection{ false };
		DMUI_ClientHandle client{ DMUI_INVALID_CLIENT_HANDLE };
		DMUI_PageHandle page{ DMUI_INVALID_PAGE_HANDLE };
		std::optional<HostPageKind> hostPage;
	};

	[[nodiscard]] NavigationResult ApplyNavigationRequest(
		const NavigationModel& a_model,
		const NavigationRequest& a_request,
		ClientSelectionState& a_state);
}
