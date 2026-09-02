#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace DearModdingUI::MCM::detail
{
	[[nodiscard]] std::string ToLowerAscii(std::string_view a_value);

	class Diagnostics
	{
	public:
		Diagnostics(
			std::string a_source,
			std::vector<Diagnostic>& a_sink) noexcept :
			m_source(std::move(a_source)),
			m_sink(a_sink)
		{}

		void Add(
			DiagnosticSeverity a_severity,
			std::string a_location,
			std::string a_message);

		void Warn(std::string a_location, std::string a_message);
		void Error(std::string a_location, std::string a_message);

		// Guarded push for the noexcept load boundary, where allocation must
		// never escape as an exception.
		void AddTerminal(std::string a_message) noexcept;

		[[nodiscard]] std::string UniqueId(
			std::string a_candidate,
			std::unordered_set<std::string>& a_ids,
			std::string_view a_kind,
			std::string_view a_location);

	private:
		std::string m_source;
		std::vector<Diagnostic>& m_sink;
	};
}
