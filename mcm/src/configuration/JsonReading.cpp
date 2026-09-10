#include "JsonReading.h"

#include <nlohmann/json.hpp>

#include <utility>

namespace DearModdingUI::MCM::detail
{
	std::optional<std::string> ReadJsonString(
		const nlohmann::json& a_object,
		std::string_view a_name,
		std::string_view a_location,
		bool a_required,
		Diagnostics& a_diagnostics)
	{
		const auto member = a_object.find(a_name);
		if (member == a_object.end())
		{
			if (a_required)
			{
				a_diagnostics.Add(
					DiagnosticSeverity::kError,
					std::string{ a_location } + "." + std::string{ a_name },
					"missing required string");
			}
			return std::nullopt;
		}
		if (!member->is_string())
		{
			a_diagnostics.Add(
				DiagnosticSeverity::kError,
				std::string{ a_location } + "." + std::string{ a_name },
				"expected a string");
			return std::nullopt;
		}
		return member->get<std::string>();
	}

	std::optional<Scalar> ReadJsonScalar(
		const nlohmann::json& a_value,
		std::string a_location,
		Diagnostics& a_diagnostics)
	{
		if (a_value.is_boolean())
			return Scalar{ a_value.get<bool>() };
		if (a_value.is_number_unsigned())
			return Scalar{ a_value.get<uint64_t>() };
		if (a_value.is_number_integer())
			return Scalar{ a_value.get<int64_t>() };
		if (a_value.is_number_float())
			return Scalar{ a_value.get<double>() };
		if (a_value.is_string())
			return Scalar{ a_value.get<std::string>() };
		a_diagnostics.Add(
			DiagnosticSeverity::kError,
			std::move(a_location),
			"expected a scalar JSON value");
		return std::nullopt;
	}
}
