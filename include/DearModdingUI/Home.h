#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace DearModdingUI
{
	struct RegisteredClient;

	struct HomeClientSection
	{
		std::string heading;
		char32_t glyph{};
		std::vector<const RegisteredClient*> clients;
	};

	[[nodiscard]] std::vector<HomeClientSection> BuildHomeClientSections(
		const std::vector<RegisteredClient>& a_clients);
}
