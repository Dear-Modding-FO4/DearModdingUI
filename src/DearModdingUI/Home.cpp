#include <DearModdingUI/Home.h>

#include <DearModdingUI/IconGlyphs.h>
#include <DearModdingUI/Registry.h>

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>

namespace DearModdingUI
{
	std::vector<HomeClientSection> BuildHomeClientSections(
		const std::vector<RegisteredClient>& a_clients)
	{
		std::vector<const RegisteredClient*> sortedClients;
		sortedClients.reserve(a_clients.size());
		for (const auto& client : a_clients)
			sortedClients.push_back(&client);
		std::ranges::sort(
			sortedClients,
			[](const auto* a_left, const auto* a_right) {
				return std::tie(a_left->displayName, a_left->id) <
					std::tie(a_right->displayName, a_right->id);
			});

		HomeClientSection native{
			"Registered mods",
			PhosphorGlyph::kPuzzlePiece,
			{}
		};
		std::map<std::string, std::vector<const RegisteredClient*>> bridged;
		for (const auto* client : sortedClients)
		{
			if (client->origin == DMUI_CLIENT_ORIGIN_NATIVE)
				native.clients.push_back(client);
			else
				bridged[client->bridgeSourceLabel].push_back(client);
		}

		std::vector<HomeClientSection> sections;
		if (!native.clients.empty() || bridged.empty())
			sections.push_back(std::move(native));
		const auto bridgeGlyph = FindPhosphorIconGlyphOrZero("share-network");
		for (auto& [sourceLabel, clients] : bridged)
		{
			sections.push_back({
				sourceLabel.empty() ?
					"Bridged mods" :
					sourceLabel + " mods",
				bridgeGlyph,
				std::move(clients)
			});
		}
		return sections;
	}
}
