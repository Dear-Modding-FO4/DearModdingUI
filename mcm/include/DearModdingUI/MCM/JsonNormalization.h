#pragma once

#include <string>
#include <string_view>

namespace DearModdingUI::MCM
{
	struct JsonNormalizationOptions
	{
		bool invalidEscapePassThrough{};
	};

	[[nodiscard]] std::string NormalizeJson(
		std::string_view a_json,
		JsonNormalizationOptions a_options = {});
}
