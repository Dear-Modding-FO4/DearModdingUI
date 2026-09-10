#pragma once

#include "../support/Diagnostics.h"

#include <DearModdingUI/MCM/Compatibility.h>

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace DearModdingUI::MCM::detail
{
	[[nodiscard]] std::optional<std::string> ReadJsonString(
		const nlohmann::json& a_object,
		std::string_view a_name,
		std::string_view a_location,
		bool a_required,
		Diagnostics& a_diagnostics);

	[[nodiscard]] std::optional<Scalar> ReadJsonScalar(
		const nlohmann::json& a_value,
		std::string a_location,
		Diagnostics& a_diagnostics);
}
