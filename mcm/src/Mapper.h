#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

namespace DearModdingUI::MCM::detail
{
	[[nodiscard]] std::string MakeIdentifier(
		std::string_view a_text,
		std::string_view a_fallback);

	void MapConfiguration(
		const Configuration& a_configuration,
		std::string_view a_source,
		std::vector<MappedPage>& a_pages,
		std::vector<Diagnostic>& a_diagnostics);
}
