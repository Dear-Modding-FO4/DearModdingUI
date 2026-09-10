#include "Diagnostics.h"

#include <utility>

namespace DearModdingUI::MCM::detail
{
	std::string ToLowerAscii(std::string_view a_value)
	{
		std::string result;
		result.reserve(a_value.size());
		for (const auto character : a_value)
		{
			auto value = static_cast<unsigned char>(character);
			if (value >= 'A' && value <= 'Z')
				value = static_cast<unsigned char>(value - 'A' + 'a');
			result.push_back(static_cast<char>(value));
		}
		return result;
	}

	void Diagnostics::Add(
		DiagnosticSeverity a_severity,
		std::string a_location,
		std::string a_message)
	{
		m_sink.push_back({
			a_severity,
			m_source,
			std::move(a_location),
			std::move(a_message)
		});
	}

	void Diagnostics::Warn(std::string a_location, std::string a_message)
	{
		Add(DiagnosticSeverity::kWarning,
			std::move(a_location),
			std::move(a_message));
	}

	void Diagnostics::Error(std::string a_location, std::string a_message)
	{
		Add(DiagnosticSeverity::kError,
			std::move(a_location),
			std::move(a_message));
	}

	void Diagnostics::AddTerminal(std::string a_message) noexcept
	{
		try
		{
			Add(DiagnosticSeverity::kError, "$", std::move(a_message));
		}
		catch (...)
		{}
	}

	std::string Diagnostics::UniqueId(
		std::string a_candidate,
		std::unordered_set<std::string>& a_ids,
		std::string_view a_kind,
		std::string_view a_location)
	{
		if (a_ids.insert(a_candidate).second)
			return a_candidate;
		auto suffix = size_t{ 2 };
		auto unique = a_candidate + "-" + std::to_string(suffix);
		while (!a_ids.insert(unique).second)
			unique = a_candidate + "-" + std::to_string(++suffix);
		Add(DiagnosticSeverity::kWarning,
			std::string{ a_location },
			"duplicate " + std::string{ a_kind } + " id '" +
				a_candidate + "' was renamed to '" + unique + "'");
		return unique;
	}
}
