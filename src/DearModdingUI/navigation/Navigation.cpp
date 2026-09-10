#include <DearModdingUI/navigation/Navigation.h>
#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/host/Registry.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

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

		[[nodiscard]] IconConceptMatch BestClientCategoryConcept(
			const NavigationClient& a_client)
		{
			IconConceptMatch best;
			for (const auto& category : a_client.categories)
			{
				const auto candidate =
					FindIconConceptMatch(category.displayName, false);
				if (PreferIconConceptMatch(candidate, best))
					best = candidate;
			}
			return best;
		}

		[[nodiscard]] std::optional<NavigationMatchQuality> MatchQuality(
			const NavigationSearchRecord& a_record,
			std::string_view a_normalizedQuery)
		{
			if (a_record.normalizedDisplayName == a_normalizedQuery)
				return NavigationMatchQuality::kDisplayNameExact;
			if (a_record.normalizedDisplayName.starts_with(a_normalizedQuery))
				return NavigationMatchQuality::kDisplayNamePrefix;
			if (a_record.normalizedDisplayName.contains(a_normalizedQuery))
				return NavigationMatchQuality::kDisplayNameSubstring;
			if (a_record.normalizedClientDisplayName.contains(
					a_normalizedQuery))
				return NavigationMatchQuality::kClientDisplayName;
			if (a_record.normalizedCategory.contains(a_normalizedQuery))
				return NavigationMatchQuality::kCategory;
			if (a_record.normalizedId.contains(a_normalizedQuery))
				return NavigationMatchQuality::kId;
			if (a_record.normalizedSummary.contains(a_normalizedQuery))
				return NavigationMatchQuality::kSummary;
			return std::nullopt;
		}

		[[nodiscard]] NavigationSearchRecord MakeSearchRecord(
			NavigationSearchEntry a_entry)
		{
			NavigationSearchRecord result;
			result.normalizedClientDisplayName =
				Lower(a_entry.clientDisplayName);
			result.normalizedId = Lower(a_entry.id);
			result.normalizedDisplayName = Lower(a_entry.displayName);
			result.normalizedCategory = Lower(a_entry.category);
			result.normalizedSummary = Lower(a_entry.summary);
			result.stableId.reserve(
				a_entry.clientId.size() + a_entry.id.size() + 8);
			result.stableId.append(a_entry.clientId);
			result.stableId.push_back('/');
			switch (a_entry.kind)
			{
			case NavigationItemKind::kClient:
				result.stableId.append("client/");
				break;
			case NavigationItemKind::kAction:
				result.stableId.append("action/");
				break;
			default:
				result.stableId.append("page/");
				break;
			}
			result.stableId.append(a_entry.id);
			result.entry = std::move(a_entry);
			return result;
		}
	}

	[[nodiscard]] char32_t ResolveNavigationClientIconGlyph(
		const NavigationClient& a_client) noexcept
	{
		try
		{
			if (const auto glyph =
					ResolveNamedIconGlyphOrZero(a_client.iconName))
				return glyph;
			const auto category = BestClientCategoryConcept(a_client);
			return ResolveClientIconGlyph(
				{},
				category.slug,
				a_client.displayName);
		}
		catch (...)
		{
			return PhosphorGlyph::kQuestion;
		}
	}

	char32_t ResolveNavigationCategoryIconGlyph(
		const NavigationClient& a_client,
		const NavigationCategory& a_category) noexcept
	{
		return ResolveCategoryIconGlyph(
			a_category.displayName,
			a_client.displayName,
			a_client.id,
			a_client.iconName,
			a_category.iconName);
	}

	const NavigationClient* NavigationModel::FindClient(
		DMUI_ClientHandle a_client) const noexcept
	{
		const auto found = std::ranges::find_if(clients, [&](const auto& a_entry) {
			return a_entry.handle == a_client;
		});
		return found != clients.end() ? &*found : nullptr;
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

	const NavigationClientSection* NavigationModel::FindSectionForClient(
		DMUI_ClientHandle a_client) const noexcept
	{
		for (const auto& section : sections)
		{
			for (const auto index : section.clientIndices)
			{
				assert(index < clients.size());
				if (clients[index].handle == a_client)
					return &section;
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

	std::string PageRowLabel(
		const NavigationClient& a_client,
		const NavigationPage& a_page)
	{
		return "###DearModdingPage/" +
			a_client.id + "/" + a_page.id;
	}

	NavigationModel BuildNavigationModel(
		const std::vector<RegisteredClient>& a_clients,
		const std::vector<RegisteredCategory>& a_categories,
		const std::vector<RegisteredPage>& a_pages,
		const std::vector<RegisteredAction>& a_actions)
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
				{},
				client->iconName,
				client->origin,
				client->bridgeSourceLabel
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
					a_left->sortKey,
					a_left->displayName,
					a_left->id) <
					std::tie(
						a_right->sortKey,
						a_right->displayName,
						a_right->id);
			});

			NavigationCategory uncategorized{};
			for (const auto* page : orderedPages)
			{
				if (!page->categoryId.empty())
					continue;
				uncategorized.pages.push_back({
					page->handle,
					page->client,
					page->id,
					page->displayName,
					{},
					page->summary,
					page->sortKey,
					{},
					page->iconName
				});
			}
			if (!uncategorized.pages.empty())
				navigationClient.categories.push_back(std::move(uncategorized));

			std::vector<const RegisteredCategory*> orderedCategories;
			for (const auto& category : a_categories)
			{
				if (category.client == client->handle)
					orderedCategories.push_back(&category);
			}
			std::ranges::sort(
				orderedCategories,
				[](const auto* a_left, const auto* a_right) {
					return std::tie(
						a_left->sortKey,
						a_left->displayName,
						a_left->id) <
						std::tie(
							a_right->sortKey,
							a_right->displayName,
							a_right->id);
				});
			for (const auto* category : orderedCategories)
			{
				NavigationCategory navigationCategory{
					category->displayName,
					{},
					category->id,
					category->sortKey,
					category->iconName
				};
				for (const auto* page : orderedPages)
				{
					if (page->categoryId != category->id)
						continue;
					navigationCategory.pages.push_back({
						page->handle,
						page->client,
						page->id,
						page->displayName,
						category->displayName,
						page->summary,
						page->sortKey,
						category->id,
						page->iconName
					});
				}
				if (!navigationCategory.pages.empty())
					navigationClient.categories.push_back(
						std::move(navigationCategory));
			}
			model.clients.push_back(std::move(navigationClient));
		}
		for (size_t clientIndex = 0; clientIndex < model.clients.size();
			++clientIndex)
		{
			const auto& client = model.clients[clientIndex];
			if (client.origin == DMUI_CLIENT_ORIGIN_NATIVE)
			{
				auto native = std::ranges::find_if(
					model.sections,
					[](const auto& a_section) {
						return a_section.origin == DMUI_CLIENT_ORIGIN_NATIVE;
					});
				if (native == model.sections.end())
				native = model.sections.insert(
					model.sections.begin(),
					{ DMUI_CLIENT_ORIGIN_NATIVE, {}, {} });
				native->clientIndices.push_back(clientIndex);
			}
			else
			{
				auto section = std::ranges::find_if(
					model.sections,
					[&](const auto& a_section) {
						return a_section.origin == client.origin &&
							a_section.bridgeSourceLabel ==
								client.bridgeSourceLabel;
					});
				if (section == model.sections.end())
				model.sections.push_back({
					client.origin,
					client.bridgeSourceLabel,
					{ clientIndex }
				});
				else
					section->clientIndices.push_back(clientIndex);
			}
		}
		std::ranges::stable_sort(
			model.sections,
			[](const auto& a_left, const auto& a_right) {
				if (a_left.origin != a_right.origin)
					return a_left.origin == DMUI_CLIENT_ORIGIN_NATIVE;
				return a_left.bridgeSourceLabel < a_right.bridgeSourceLabel;
			});
		model.BuildSearchIndex(a_actions);
		return model;
	}

	std::string NavigationClientSectionLabel(
		DMUI_ClientOrigin a_origin,
		std::string_view a_bridgeSourceLabel)
	{
		if (a_origin == DMUI_CLIENT_ORIGIN_NATIVE)
			return "Native";
		return a_bridgeSourceLabel.empty() ?
			"Bridged" :
			std::string{ a_bridgeSourceLabel };
	}

	void NavigationModel::BuildSearchIndex(
		const std::vector<RegisteredAction>& a_actions)
	{
		m_searchIndex.clear();
		auto entryCount = clients.size() + a_actions.size();
		for (const auto& client : clients)
		{
			for (const auto& category : client.categories)
				entryCount += category.pages.size();
		}
		m_searchIndex.reserve(entryCount);
		for (const auto& client : clients)
		{
			const auto category = BestClientCategoryConcept(client);
			m_searchIndex.push_back(MakeSearchRecord({
				NavigationItemKind::kClient,
				client.handle,
				DMUI_INVALID_PAGE_HANDLE,
				DMUI_INVALID_ACTION_HANDLE,
				client.id,
				client.displayName,
				client.id,
				client.displayName,
				client.iconName,
				std::string{ category.slug },
				{},
				0
			}));
			for (const auto& category : client.categories)
			{
				for (const auto& page : category.pages)
				{
					m_searchIndex.push_back(MakeSearchRecord({
						NavigationItemKind::kPage,
						client.handle,
						page.handle,
						DMUI_INVALID_ACTION_HANDLE,
						client.id,
						client.displayName,
						page.id,
						page.displayName,
						page.iconName,
						page.categoryDisplayName,
						page.summary,
						page.sortKey
					}));
				}
			}
		}

		for (const auto& action : a_actions)
		{
			m_searchIndex.push_back(MakeSearchRecord({
				NavigationItemKind::kAction,
				action.client,
				DMUI_INVALID_PAGE_HANDLE,
				action.handle,
				action.clientId,
				action.clientDisplayName,
				action.id,
				action.displayLabel,
				action.iconName,
				{},
				action.tooltip,
				action.sortKey
			}));
		}
	}

	char32_t ResolveNavigationSearchEntryGlyph(
		const NavigationSearchEntry& a_entry) noexcept
	{
		switch (a_entry.kind)
		{
		case NavigationItemKind::kClient:
			return ResolveClientIconGlyph(
				a_entry.iconName,
				a_entry.category,
				a_entry.clientDisplayName);
		case NavigationItemKind::kAction:
			return ResolveSemanticIconGlyph(
				a_entry.iconName,
				a_entry.displayName,
				{},
				PhosphorGlyph::kTerminalWindow);
		default:
			return ResolveSemanticIconGlyph(
				a_entry.iconName,
				a_entry.displayName,
				a_entry.category,
				PhosphorGlyph::kFiles);
		}
	}

	std::vector<NavigationSearchHit> SearchNavigation(
		const NavigationModel& a_model,
		std::string_view a_query)
	{
		std::vector<NavigationSearchHit> hits;
		if (a_query.empty())
			return hits;

		const auto normalizedQuery = Lower(a_query);
		for (const auto& record : a_model.SearchIndex())
		{
			if (const auto match = MatchQuality(record, normalizedQuery))
				hits.push_back({ &record, *match });
		}
		std::ranges::sort(hits, [](const auto& a_left, const auto& a_right) {
			if (a_left.match != a_right.match)
				return a_left.match > a_right.match;
			if (a_left.Entry().sortKey != a_right.Entry().sortKey)
				return a_left.Entry().sortKey < a_right.Entry().sortKey;
			return a_left.record->stableId < a_right.record->stableId;
		});
		return hits;
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
