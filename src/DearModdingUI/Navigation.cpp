#include <DearModdingUI/Navigation.h>
#include <DearModdingUI/Registry.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <tuple>

namespace DearModdingUI
{
	namespace
	{
		[[nodiscard]] std::string Lower(std::string_view a_value)
		{
			std::string result{ a_value };
			std::ranges::transform(result, result.begin(), [](unsigned char a_character) {
				return static_cast<char>(std::tolower(a_character));
			});
			return result;
		}

		[[nodiscard]] std::optional<NavigationMatchQuality> MatchQuality(
			const NavigationSearchEntry& a_entry,
			std::string_view a_query)
		{
			const auto query = Lower(a_query);
			const auto displayName = Lower(a_entry.displayName);
			if (displayName == query)
				return NavigationMatchQuality::kDisplayNameExact;
			if (displayName.starts_with(query))
				return NavigationMatchQuality::kDisplayNamePrefix;
			if (displayName.contains(query))
				return NavigationMatchQuality::kDisplayNameSubstring;
			if (Lower(a_entry.clientDisplayName).contains(query))
				return NavigationMatchQuality::kClientDisplayName;
			if (Lower(a_entry.category).contains(query))
				return NavigationMatchQuality::kCategory;
			if (Lower(a_entry.id).contains(query))
				return NavigationMatchQuality::kId;
			if (Lower(a_entry.summary).contains(query))
				return NavigationMatchQuality::kSummary;
			return std::nullopt;
		}

		[[nodiscard]] std::string StableSearchId(
			const NavigationSearchEntry& a_entry)
		{
			std::string result;
			result.reserve(
				a_entry.clientId.size() + a_entry.id.size() + 8);
			result.append(a_entry.clientId);
			result.push_back('/');
			result.append(
				a_entry.kind == NavigationItemKind::kPage ?
					"page/" :
					"action/");
			result.append(a_entry.id);
			return result;
		}
	}

	const NavigationClient* NavigationModel::FindClient(
		DMUI_ClientHandle a_client) const noexcept
	{
		const auto found = std::ranges::find_if(clients, [&](const auto& a_entry) {
			return a_entry.handle == a_client;
		});
		return found != clients.end() ? &*found : nullptr;
	}

	const NavigationClient* NavigationModel::FindClientForPage(
		DMUI_PageHandle a_page) const noexcept
	{
		for (const auto& client : clients)
		{
			for (const auto& category : client.categories)
			{
				if (std::ranges::any_of(category.pages, [&](const auto& a_entry) {
						return a_entry.handle == a_page;
					}))
					return &client;
			}
		}
		return nullptr;
	}

	const NavigationPage* NavigationModel::FindPage(DMUI_PageHandle a_page) const noexcept
	{
		for (const auto& client : clients)
		{
			for (const auto& category : client.categories)
			{
				const auto found = std::ranges::find_if(category.pages, [&](const auto& a_entry) {
					return a_entry.handle == a_page;
				});
				if (found != category.pages.end())
					return &*found;
			}
		}
		return nullptr;
	}

	DMUI_PageHandle NavigationModel::FirstPage() const noexcept
	{
		for (const auto& client : clients)
		{
			for (const auto& category : client.categories)
			{
				if (!category.pages.empty())
					return category.pages.front().handle;
			}
		}
		return DMUI_INVALID_PAGE_HANDLE;
	}

	NavigationModel BuildNavigationModel(
		const std::vector<RegisteredClient>& a_clients,
		const std::vector<RegisteredPage>& a_pages)
	{
		std::vector<const RegisteredClient*> orderedClients;
		orderedClients.reserve(a_clients.size());
		for (const auto& client : a_clients)
		{
			if (std::ranges::any_of(a_pages, [&](const auto& a_page) {
					return a_page.client == client.handle &&
						a_page.kind == DMUI_PAGE_KIND_SETTINGS;
				}))
				orderedClients.push_back(&client);
		}
		std::ranges::sort(orderedClients, [](const auto* a_left, const auto* a_right) {
			return std::tie(a_left->displayName, a_left->id) <
				std::tie(a_right->displayName, a_right->id);
		});

		NavigationModel model;
		model.clients.reserve(orderedClients.size());
		for (const auto* client : orderedClients)
		{
			NavigationClient navigationClient{
				client->handle,
				client->id,
				client->displayName,
				client->version,
				{}
			};

			std::vector<const RegisteredPage*> orderedPages;
			for (const auto& page : a_pages)
			{
				if (page.client == client->handle &&
					page.kind == DMUI_PAGE_KIND_SETTINGS)
					orderedPages.push_back(&page);
			}
			std::ranges::sort(orderedPages, [](const auto* a_left, const auto* a_right) {
				return std::tie(
					a_left->category,
					a_left->sortKey,
					a_left->displayName,
					a_left->id) <
					std::tie(
						a_right->category,
						a_right->sortKey,
						a_right->displayName,
						a_right->id);
			});

			for (const auto* page : orderedPages)
			{
				if (navigationClient.categories.empty() ||
					navigationClient.categories.back().displayName != page->category)
					navigationClient.categories.push_back({ page->category, {} });
				navigationClient.categories.back().pages.push_back({
					page->handle,
					page->client,
					page->id,
					page->displayName,
					page->category,
					page->summary,
					page->sortKey
				});
			}
			model.clients.push_back(std::move(navigationClient));
		}
		return model;
	}

	std::vector<NavigationSearchEntry> BuildNavigationSearchIndex(
		const NavigationModel& a_model,
		const std::vector<RegisteredAction>& a_actions)
	{
		std::vector<NavigationSearchEntry> entries;
		for (const auto& client : a_model.clients)
		{
			for (const auto& category : client.categories)
			{
				for (const auto& page : category.pages)
				{
					entries.push_back({
						NavigationItemKind::kPage,
						client.handle,
						page.handle,
						DMUI_INVALID_ACTION_HANDLE,
						client.id,
						client.displayName,
						page.id,
						page.displayName,
						page.category,
						page.summary,
						page.sortKey
					});
				}
			}
		}

		for (const auto& action : a_actions)
		{
			entries.push_back({
				NavigationItemKind::kAction,
				action.client,
				DMUI_INVALID_PAGE_HANDLE,
				action.handle,
				action.clientId,
				action.clientDisplayName,
				action.id,
				action.displayLabel,
				{},
				action.tooltip,
				action.sortKey
			});
		}
		return entries;
	}

	std::vector<NavigationSearchHit> SearchNavigation(
		const NavigationModel& a_model,
		const std::vector<RegisteredAction>& a_actions,
		std::string_view a_query)
	{
		std::vector<NavigationSearchHit> hits;
		if (a_query.empty())
			return hits;

		for (auto& entry : BuildNavigationSearchIndex(a_model, a_actions))
		{
			if (const auto match = MatchQuality(entry, a_query))
				hits.push_back({ std::move(entry), *match });
		}
		std::ranges::sort(hits, [](const auto& a_left, const auto& a_right) {
			if (a_left.match != a_right.match)
				return a_left.match > a_right.match;
			if (a_left.entry.sortKey != a_right.entry.sortKey)
				return a_left.entry.sortKey < a_right.entry.sortKey;
			return StableSearchId(a_left.entry) < StableSearchId(a_right.entry);
		});
		return hits;
	}

	DMUI_PageHandle ResolvePageSelection(
		const NavigationModel& a_model,
		DMUI_PageHandle a_requested,
		DMUI_PageHandle a_current) noexcept
	{
		if (a_model.FindPage(a_requested))
			return a_requested;
		if (a_model.FindPage(a_current))
			return a_current;
		return a_model.FirstPage();
	}

	bool SelectClient(
		const NavigationModel& a_model,
		DMUI_ClientHandle a_client,
		ClientSelectionState& a_state) noexcept
	{
		const auto* client = a_model.FindClient(a_client);
		if (!client || a_state.activeClient == a_client)
			return false;

		a_state.activeClient = a_client;
		a_state.activePage = DMUI_INVALID_PAGE_HANDLE;
		for (const auto& category : client->categories)
		{
			if (!category.pages.empty())
			{
				a_state.activePage = category.pages.front().handle;
				break;
			}
		}
		a_state.search.clear();
		return true;
	}

	void RecordRecentPage(
		const NavigationModel& a_model,
		DMUI_PageHandle a_page,
		ClientSelectionState& a_state,
		size_t a_capacity)
	{
		if (a_capacity == 0 || !a_model.FindPage(a_page))
			return;
		std::erase(a_state.recentPages, a_page);
		a_state.recentPages.insert(a_state.recentPages.begin(), a_page);
		if (a_state.recentPages.size() > a_capacity)
			a_state.recentPages.resize(a_capacity);
	}

	void PruneRecentPages(
		const NavigationModel& a_model,
		ClientSelectionState& a_state)
	{
		std::erase_if(a_state.recentPages, [&](const auto a_page) {
			return !a_model.FindPage(a_page);
		});
	}

	DMUI_PageHandle ResolveLandingPage(
		const NavigationClient& a_client) noexcept
	{
		const NavigationPage* landing{};
		for (const auto& category : a_client.categories)
		{
			for (const auto& page : category.pages)
			{
				if (!landing ||
					std::tie(page.sortKey, page.id) <
						std::tie(landing->sortKey, landing->id))
					landing = &page;
			}
		}
		return landing ? landing->handle : DMUI_INVALID_PAGE_HANDLE;
	}

	PagePresentation DecidePagePresentation(
		const NavigationPage* a_page,
		bool a_callbackFailed) noexcept
	{
		if (!a_page)
			return PagePresentation::kEmpty;
		return a_callbackFailed ? PagePresentation::kFailure : PagePresentation::kContent;
	}
}
