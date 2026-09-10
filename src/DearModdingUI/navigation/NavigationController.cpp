#include <DearModdingUI/navigation/NavigationController.h>

namespace DearModdingUI
{
	NavigationResult ApplyNavigationRequest(
		const NavigationModel& a_model,
		const NavigationRequest& a_request,
		ClientSelectionState& a_state)
	{
		const auto previousClient = a_state.activeClient;
		const auto previousPage = a_state.activePage;
		const auto previousHost = a_state.activeHostPage;

		switch (a_request.kind)
		{
		case NavigationRequestKind::Host:
			if (!FindHostNavigationPage(a_request.hostPage))
				return {};
			SelectHostPage(a_request.hostPage, a_state);
			return {
				true,
				previousClient != a_state.activeClient ||
					previousPage != a_state.activePage ||
					previousHost != a_state.activeHostPage,
				true,
				a_state.activeClient,
				a_state.activePage,
				a_state.activeHostPage
			};
		case NavigationRequestKind::Page:
			if (const auto* page = a_model.FindPage(a_request.page))
			{
				a_state.activeClient = page->client;
				a_state.activePage = page->handle;
			}
			else
				return {};
			break;
		case NavigationRequestKind::Client:
			if (const auto* client = a_model.FindClient(a_request.client))
			{
				const auto page = ResolveLandingPage(*client);
				if (!a_model.FindPage(page))
					return {};
				a_state.activeClient = client->handle;
				a_state.activePage = page;
			}
			else
				return {};
			break;
		default:
			return {};
		}
		a_state.activeHostPage.reset();
		a_state.activeHostPage.reset();
		RecordRecentPage(a_model, a_state.activePage, a_state);
		return {
			true,
			previousClient != a_state.activeClient ||
				previousPage != a_state.activePage ||
				previousHost != a_state.activeHostPage,
			true,
			a_state.activeClient,
			a_state.activePage,
			std::nullopt
		};
	}
}
