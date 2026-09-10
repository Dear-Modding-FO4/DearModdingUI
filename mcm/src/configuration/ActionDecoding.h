#pragma once

#include "../support/Diagnostics.h"

#include <DearModdingUI/MCM/Compatibility.h>

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>

namespace DearModdingUI::MCM::detail
{
	[[nodiscard]] std::optional<Action> DecodeAction(
		const nlohmann::json& a_action,
		const std::string& a_location,
		Diagnostics& a_diagnostics);
}
