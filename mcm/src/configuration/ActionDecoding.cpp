#include "ActionDecoding.h"

#include "JsonReading.h"

#include <nlohmann/json.hpp>

#include <charconv>
#include <utility>

namespace DearModdingUI::MCM::detail
{
	namespace
	{
		[[nodiscard]] std::optional<ActionArgument> DecodeActionArgument(
			const nlohmann::json& a_value,
			std::string a_location,
			Diagnostics& a_diagnostics)
		{
			if (!a_value.is_string())
			{
				auto scalar = ReadJsonScalar(
					a_value,
					std::move(a_location),
					a_diagnostics);
				if (!scalar)
					return std::nullopt;
				return std::visit(
					[](auto a_scalar) -> ActionArgument {
						return std::move(a_scalar);
					},
					std::move(*scalar));
			}

			auto value = a_value.get<std::string>();
			auto type = SourceValueKind::kNone;
			if (value.size() >= 3 && value.front() == '{' &&
				value[2] == '}')
			{
				switch (value[1])
				{
				case 'i':
					type = SourceValueKind::kInt;
					break;
				case 'f':
					type = SourceValueKind::kFloat;
					break;
				case 'b':
					type = SourceValueKind::kBool;
					break;
				case 's':
					type = SourceValueKind::kString;
					break;
				default:
					break;
				}
				if (type != SourceValueKind::kNone)
					value.erase(0, 3);
			}
			if (value == "{value}")
				return ActionArgument{ ValueArgument{ type } };
			if (value.find("{value}") != std::string::npos)
			{
				return ActionArgument{
					ValueTemplateArgument{ std::move(value), type }
				};
			}
			switch (type)
			{
			case SourceValueKind::kInt:
			{
				int64_t parsed{};
				const auto converted = std::from_chars(
					value.data(),
					value.data() + value.size(),
					parsed);
				if (converted.ec == std::errc{} &&
					converted.ptr == value.data() + value.size())
					return ActionArgument{ parsed };
				break;
			}
			case SourceValueKind::kFloat:
			{
				double parsed{};
				const auto converted = std::from_chars(
					value.data(),
					value.data() + value.size(),
					parsed);
				if (converted.ec == std::errc{} &&
					converted.ptr == value.data() + value.size())
					return ActionArgument{ parsed };
				break;
			}
			case SourceValueKind::kBool:
				if (value == "true")
					return ActionArgument{ true };
				if (value == "false")
					return ActionArgument{ false };
				break;
			case SourceValueKind::kString:
			case SourceValueKind::kNone:
				return ActionArgument{ std::move(value) };
			}
			a_diagnostics.Add(
				DiagnosticSeverity::kWarning,
				std::move(a_location),
				"invalid typed action argument");
			return std::nullopt;
		}

		[[nodiscard]] std::vector<ActionArgument> DecodeActionArguments(
			const nlohmann::json& a_action,
			const std::string& a_location,
			Diagnostics& a_diagnostics)
		{
			std::vector<ActionArgument> result;
			const auto params = a_action.find("params");
			if (params == a_action.end())
				return result;
			if (!params->is_array())
			{
				a_diagnostics.Add(
					DiagnosticSeverity::kError,
					a_location + ".params",
					"expected an array");
				return result;
			}
			for (size_t index = 0; index < params->size(); ++index)
			{
				if (auto argument = DecodeActionArgument(
						(*params)[index],
						a_location + ".params[" +
							std::to_string(index) + "]",
						a_diagnostics))
					result.push_back(std::move(*argument));
			}
			return result;
		}
	}

	std::optional<Action> DecodeAction(
		const nlohmann::json& a_action,
		const std::string& a_location,
		Diagnostics& a_diagnostics)
	{
		if (!a_action.is_object())
		{
			a_diagnostics.Add(
				DiagnosticSeverity::kError,
				a_location,
				"expected an action object");
			return std::nullopt;
		}
		const auto type = ReadJsonString(
			a_action,
			"type",
			a_location,
			true,
			a_diagnostics);
		if (!type)
			return std::nullopt;
		auto arguments =
			DecodeActionArguments(a_action, a_location, a_diagnostics);
		const auto readString = [&](
			std::string_view a_name,
			bool a_required = false) {
			return ReadJsonString(
				a_action,
				a_name,
				a_location,
				a_required,
				a_diagnostics);
		};
		if (*type == "CallFunction")
		{
			return Action{ CallFunctionAction{
				readString("form", true).value_or(std::string{}),
				readString("scriptName"),
				readString("function", true).value_or(std::string{}),
				std::move(arguments)
			} };
		}
		if (*type == "CallGlobalFunction")
		{
			return Action{ CallGlobalFunctionAction{
				readString("script", true).value_or(std::string{}),
				readString("function", true).value_or(std::string{}),
				std::move(arguments)
			} };
		}
		if (*type == "CallExternalFunction")
		{
			return Action{ CallExternalFunctionAction{
				readString("plugin", true).value_or(std::string{}),
				readString("function", true).value_or(std::string{}),
				std::move(arguments)
			} };
		}
		if (*type == "RunConsoleCommand")
		{
			return Action{ RunConsoleCommandAction{
				readString("command", true).value_or(std::string{})
			} };
		}
		if (*type == "SendEvent")
		{
			auto event = readString("event");
			if (!event)
				event = readString("eventName", true);
			return Action{ SendEventAction{
				event.value_or(std::string{}),
				std::move(arguments)
			} };
		}
		a_diagnostics.Add(
			DiagnosticSeverity::kWarning,
			a_location + ".type",
			"unknown MCM action type '" + *type + "'");
		return std::nullopt;
	}
}
