#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <string>
#include <string_view>

namespace DearModdingUI::MCM::detail
{
	[[nodiscard]] ControlType DecodeControlType(std::string_view a_type);
	[[nodiscard]] SourceType DecodeSourceType(std::string a_raw);
	[[nodiscard]] bool NeedsValueOptions(ControlType a_type) noexcept;
}
