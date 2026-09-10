#include "NavigationFixtures.h"

#include <algorithm>
#include <exception>
#include <span>
#include <string_view>

namespace DmuiTestFixtures
{
	namespace
	{
		[[nodiscard]] bool AddPages(
			dmui::Client& a_client,
			std::span<const NavigationFixturePage> a_pages,
			std::string& a_error)
		{
			std::vector<std::string_view> categories;
			for (const auto& page : a_pages)
			{
				if (!page.categoryId)
					continue;
				const auto existing = std::ranges::find(
					categories,
					std::string_view{ page.categoryId });
				if (existing != categories.end())
					continue;
				if (!a_client.AddCategory({
						.id = page.categoryId,
						.displayName = page.categoryDisplayName
					}))
				{
					a_error = "Could not register navigation category " +
						std::string{ page.categoryId } + " (result " +
						DMUI_ResultToString(a_client.LastResult()) + ").";
					return false;
				}
				categories.push_back(page.categoryId);
			}

			int32_t sortKey{};
			for (const auto& page : a_pages)
			{
				const auto handle = a_client.AddPage(
					{
						.id = page.id,
						.displayName = page.displayName,
						.categoryId = page.categoryId,
						.summary = page.summary,
						.sortKey = sortKey
					},
					[client = &a_client,
					 name = std::string{ page.displayName },
					 summary = std::string{ page.summary }] {
						(void)client->DrawSectionHeader(name.c_str());
						(void)client->DrawBulletText(summary.c_str());
						(void)client->DrawBulletText(
							"Dedicated preview-only navigation layout fixture.");
					});
				if (!handle)
				{
					a_error = "Could not register navigation page " +
						std::string{ page.id } + " (result " +
						DMUI_ResultToString(a_client.LastResult()) + ").";
					return false;
				}
				sortKey += 10;
			}
			return true;
		}
	}

	bool RegisterNavigationComparisonFixtures(
		std::vector<std::unique_ptr<dmui::Client>>& a_clients,
		std::string& a_error) noexcept
	{
		try
		{
			a_error.clear();
			a_clients.reserve(
				a_clients.size() + kNavigationFixtureClients.size());
			for (const auto& fixture : kNavigationFixtureClients)
			{
				auto client = std::make_unique<dmui::Client>(
					fixture.id,
					fixture.displayName,
					dmui::Version{ 0, 1 },
					"share-network",
					dmui::ClientOrigin{
						dmui::ClientOriginKind::kBridged,
						fixture.source
					});
				if (!client->Connect())
				{
					a_error = "Could not connect navigation fixture " +
						std::string{ fixture.id } + " (result " +
						DMUI_ResultToString(client->LastResult()) + ").";
					return false;
				}
				auto* registered = client.get();
				a_clients.push_back(std::move(client));
				if (!AddPages(*registered, kNavigationFixturePages, a_error))
					return false;
			}
			return true;
		}
		catch (const std::exception& a_exception)
		{
			a_error = "Navigation fixture registration threw: ";
			a_error += a_exception.what();
			return false;
		}
	}
}
