#include <DearModdingUI/MCM/Compatibility.h>
#include <DearModdingUI/MCM/JsonNormalization.h>

#include "Diagnostics.h"
#include "Mapper.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace DearModdingUI::MCM
{
	namespace
	{
		using Json = nlohmann::json;

		constexpr size_t kMaxConditionDepth = 64;

		[[nodiscard]] ControlType ResolveControlType(std::string_view a_type)
		{
			const auto type = detail::ToLowerAscii(a_type);
			if (type == "switch" || type == "switcher")
				return ControlType::kSwitch;
			if (type == "slider")
				return ControlType::kSlider;
			if (type == "stepper")
				return ControlType::kStepper;
			if (type == "menu" || type == "enum" || type == "dropdown")
				return ControlType::kMenu;
			if (type == "input" || type == "textinput")
				return ControlType::kInput;
			if (type == "text")
				return ControlType::kText;
			if (type == "header" || type == "section")
				return ControlType::kGroup;
			if (type == "empty" || type == "spacer")
				return ControlType::kSpacing;
			if (type == "hidden" || type == "hiddenswitcher")
				return ControlType::kHidden;
			if (type == "button")
				return ControlType::kButton;
			if (type == "keymap" || type == "hotkey")
				return ControlType::kKeymap;
			if (type == "color")
				return ControlType::kColor;
			if (type == "image")
				return ControlType::kImage;
			return ControlType::kUnknown;
		}

		[[nodiscard]] SourceType ResolveSourceType(std::string a_raw)
		{
			SourceType result;
			const auto type = detail::ToLowerAscii(a_raw);
			result.raw = std::move(a_raw);
			if (type.starts_with("globalvalue"))
				result.family = SourceFamily::kGlobal;
			else if (type.starts_with("propertyvalue"))
				result.family = SourceFamily::kProperty;
			else if (type.starts_with("modsetting"))
				result.family = SourceFamily::kModSetting;
			else
				return result;
			if (type.ends_with("bool"))
				result.value = SourceValueKind::kBool;
			else if (type.ends_with("int"))
				result.value = SourceValueKind::kInt;
			else if (type.ends_with("float"))
				result.value = SourceValueKind::kFloat;
			else if (type.ends_with("string"))
				result.value = SourceValueKind::kString;
			return result;
		}

		[[nodiscard]] bool NeedsValueOptions(ControlType a_type) noexcept
		{
			switch (a_type)
			{
			case ControlType::kSwitch:
			case ControlType::kSlider:
			case ControlType::kStepper:
			case ControlType::kMenu:
			case ControlType::kInput:
				return true;
			default:
				return false;
			}
		}

		void AddTerminalDiagnostic(
			LoadResult& a_result,
			std::string_view a_source,
			std::string a_message) noexcept
		{
			detail::Diagnostics{ std::string{ a_source }, a_result.diagnostics }
				.AddTerminal(std::move(a_message));
		}

		class ConfigReader
		{
		public:
			ConfigReader(
				std::string a_source,
				LoadResult& a_result) :
				m_source(std::move(a_source)),
				m_result(a_result),
				m_diagnostics(m_source, a_result.diagnostics)
			{}

			void Read(const Json& a_document)
			{
				if (!a_document.is_object())
				return Diagnose(
					DiagnosticSeverity::kError,
					"$",
					"MCM configuration root must be an object");

				m_result.configuration.emplace();
				auto& configuration = *m_result.configuration;

				// Configs write this as a marketing version as often as a version code.
				if (const auto value = ReadNumber(
						a_document, "minMcmVersion", "$"))
					configuration.minimumMcmVersion = static_cast<int64_t>(*value);
				if (const auto value = ReadString(
						a_document, "modName", "$", true))
					configuration.modName = *value;
				if (const auto value = ReadString(
						a_document, "displayName", "$", true))
					configuration.displayName = *value;

				ReadRequirements(a_document, configuration);
				ReadRootPage(a_document, configuration);
				ReadPages(a_document, configuration);

				if (configuration.pages.empty())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						"$",
						"configuration produced no pages");
				}
			}

		private:
			void Diagnose(
				DiagnosticSeverity a_severity,
				std::string a_location,
				std::string a_message)
			{
				m_diagnostics.Add(
					a_severity,
					std::move(a_location),
					std::move(a_message));
			}

			[[nodiscard]] std::optional<std::string> ReadString(
				const Json& a_object,
				std::string_view a_name,
				std::string_view a_location,
				bool a_required = false)
			{
				const auto member = a_object.find(a_name);
				if (member == a_object.end())
				{
					if (a_required)
					{
						Diagnose(
							DiagnosticSeverity::kError,
							std::string{ a_location } + "." + std::string{ a_name },
							"missing required string");
					}
					return std::nullopt;
				}
				if (!member->is_string())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						std::string{ a_location } + "." + std::string{ a_name },
						"expected a string");
					return std::nullopt;
				}
				return member->get<std::string>();
			}

			[[nodiscard]] std::optional<int64_t> ReadInteger(
				const Json& a_object,
				std::string_view a_name,
				std::string_view a_location)
			{
				const auto member = a_object.find(a_name);
				if (member == a_object.end())
					return std::nullopt;
				if (member->is_number_unsigned())
				{
					const auto value = member->get<uint64_t>();
					if (value <= static_cast<uint64_t>(
							(std::numeric_limits<int64_t>::max)()))
						return static_cast<int64_t>(value);
				}
				else if (member->is_number_integer())
				{
					return member->get<int64_t>();
				}
				Diagnose(
					DiagnosticSeverity::kError,
					std::string{ a_location } + "." + std::string{ a_name },
					"expected a signed 64-bit integer");
				return std::nullopt;
			}

			[[nodiscard]] std::optional<double> ReadNumber(
				const Json& a_object,
				std::string_view a_name,
				std::string_view a_location)
			{
				const auto member = a_object.find(a_name);
				if (member == a_object.end())
					return std::nullopt;
				if (!member->is_number())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						std::string{ a_location } + "." + std::string{ a_name },
						"expected a number");
					return std::nullopt;
				}
				return member->get<double>();
			}

			[[nodiscard]] std::optional<bool> ReadBoolean(
				const Json& a_object,
				std::string_view a_name,
				std::string_view a_location)
			{
				const auto member = a_object.find(a_name);
				if (member == a_object.end())
					return std::nullopt;
				if (!member->is_boolean())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						std::string{ a_location } + "." + std::string{ a_name },
						"expected a boolean");
					return std::nullopt;
				}
				return member->get<bool>();
			}

			[[nodiscard]] std::optional<Scalar> ReadScalar(
				const Json& a_value,
				std::string a_location)
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
				Diagnose(
					DiagnosticSeverity::kError,
					std::move(a_location),
					"expected a scalar JSON value");
				return std::nullopt;
			}

			[[nodiscard]] std::optional<ActionArgument> ReadActionArgument(
				const Json& a_value,
				std::string a_location)
			{
				if (!a_value.is_string())
				{
					auto scalar = ReadScalar(a_value, std::move(a_location));
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
					return ActionArgument{ std::move(value) };
				case SourceValueKind::kNone:
					return ActionArgument{ std::move(value) };
				}
				Diagnose(
					DiagnosticSeverity::kWarning,
					std::move(a_location),
					"invalid typed action argument");
				return std::nullopt;
			}

			[[nodiscard]] std::vector<ActionArgument> ReadActionArguments(
				const Json& a_action,
				const std::string& a_location)
			{
				std::vector<ActionArgument> result;
				const auto params = a_action.find("params");
				if (params == a_action.end())
					return result;
				if (!params->is_array())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						a_location + ".params",
						"expected an array");
					return result;
				}
				for (size_t index = 0; index < params->size(); ++index)
				{
					if (auto argument = ReadActionArgument(
							(*params)[index],
							a_location + ".params[" +
								std::to_string(index) + "]"))
						result.push_back(std::move(*argument));
				}
				return result;
			}

			[[nodiscard]] std::optional<Action> ReadAction(
				const Json& a_action,
				const std::string& a_location)
			{
				if (!a_action.is_object())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						a_location,
						"expected an action object");
					return std::nullopt;
				}
				const auto type = ReadString(a_action, "type", a_location, true);
				if (!type)
					return std::nullopt;
				auto arguments = ReadActionArguments(a_action, a_location);
				if (*type == "CallFunction")
				{
					return Action{ CallFunctionAction{
						ReadString(a_action, "form", a_location, true)
							.value_or(std::string{}),
						ReadString(a_action, "scriptName", a_location),
						ReadString(a_action, "function", a_location, true)
							.value_or(std::string{}),
						std::move(arguments)
					} };
				}
				if (*type == "CallGlobalFunction")
				{
					return Action{ CallGlobalFunctionAction{
						ReadString(a_action, "script", a_location, true)
							.value_or(std::string{}),
						ReadString(a_action, "function", a_location, true)
							.value_or(std::string{}),
						std::move(arguments)
					} };
				}
				if (*type == "CallExternalFunction")
				{
					return Action{ CallExternalFunctionAction{
						ReadString(a_action, "plugin", a_location, true)
							.value_or(std::string{}),
						ReadString(a_action, "function", a_location, true)
							.value_or(std::string{}),
						std::move(arguments)
					} };
				}
				if (*type == "RunConsoleCommand")
				{
					return Action{ RunConsoleCommandAction{
						ReadString(a_action, "command", a_location, true)
							.value_or(std::string{})
					} };
				}
				if (*type == "SendEvent")
				{
					auto event = ReadString(a_action, "event", a_location);
					if (!event)
						event = ReadString(
							a_action,
							"eventName",
							a_location,
							true);
					return Action{ SendEventAction{
						event.value_or(std::string{}),
						std::move(arguments)
					} };
				}
				Diagnose(
					DiagnosticSeverity::kWarning,
					a_location + ".type",
					"unknown MCM action type '" + *type + "'");
				return std::nullopt;
			}

			[[nodiscard]] std::optional<GroupCondition> ReadCondition(
				const Json& a_value,
				const std::string& a_location,
				size_t a_depth = 0)
			{
				if (a_value.is_number_unsigned())
				{
					const auto value = a_value.get<uint64_t>();
					if (value <= static_cast<uint64_t>(
							(std::numeric_limits<int64_t>::max)()))
					{
						return GroupCondition{
							ConditionType::kControl,
							static_cast<int64_t>(value)
						};
					}
				}
				else if (a_value.is_number_integer())
				{
					return GroupCondition{
						ConditionType::kControl,
						a_value.get<int64_t>()
					};
				}
				if (!a_value.is_object() || a_value.empty())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						a_location,
						"expected a control number or condition object");
					return std::nullopt;
				}

				if (a_value.size() != 1)
				{
					Diagnose(
						DiagnosticSeverity::kError,
						a_location,
						"condition object must have one operator");
				}
				const auto operation = a_value.begin();
				GroupCondition result;
				result.rawOperator = operation.key();
				const auto normalized = detail::ToLowerAscii(result.rawOperator);
				if (normalized == "and")
					result.type = ConditionType::kAll;
				else if (normalized == "or")
					result.type = ConditionType::kAny;
				else
				{
					Diagnose(
						DiagnosticSeverity::kWarning,
						a_location,
						"unknown condition operator '" +
							result.rawOperator + "'");
				}

				if (!operation->is_array())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						a_location + "." + result.rawOperator,
						"condition operands must be an array");
					return result;
				}
				if (a_depth >= kMaxConditionDepth)
				{
					Diagnose(
						DiagnosticSeverity::kWarning,
						a_location + "." + result.rawOperator,
						"condition nesting exceeds " +
							std::to_string(kMaxConditionDepth) +
							" levels and was truncated");
					return result;
				}
				for (size_t index = 0; index < operation->size(); ++index)
				{
					if (auto operand = ReadCondition(
							(*operation)[index],
							a_location + "." + result.rawOperator +
								"[" + std::to_string(index) + "]",
							a_depth + 1))
						result.operands.push_back(std::move(*operand));
				}
				return result;
			}

			void ReadRequirements(
				const Json& a_document,
				Configuration& a_configuration)
			{
				const auto requirements = a_document.find("pluginRequirements");
				if (requirements == a_document.end())
				return;
				if (!requirements->is_array())
				{
					return Diagnose(
						DiagnosticSeverity::kError,
						"$.pluginRequirements",
						"expected an array");
				}
				for (size_t index = 0; index < requirements->size(); ++index)
				{
					const auto& requirement = (*requirements)[index];
					if (!requirement.is_string())
					{
						Diagnose(
							DiagnosticSeverity::kError,
							"$.pluginRequirements[" + std::to_string(index) + "]",
							"expected a plugin name string");
						continue;
					}
					a_configuration.pluginRequirements.push_back(
						requirement.get<std::string>());
				}
			}

			void ReadRootPage(
				const Json& a_document,
				Configuration& a_configuration)
			{
				const auto content = a_document.find("content");
				if (content == a_document.end())
				{
					return Diagnose(
						DiagnosticSeverity::kError,
						"$.content",
						"missing required content array");
				}

				Page page;
				page.id = UniquePageId("main", "$.content");
				page.displayName = a_configuration.displayName.empty() ?
					a_configuration.modName :
					a_configuration.displayName;
				if (page.displayName.empty())
					page.displayName = "MCM";
				page.location = "$.content";
				page.root = true;
				ReadContent(*content, "$.content", page);
				a_configuration.pages.push_back(std::move(page));
			}

			void ReadPages(
				const Json& a_document,
				Configuration& a_configuration)
			{
				const auto pages = a_document.find("pages");
				if (pages == a_document.end())
					return;
				if (!pages->is_array())
				{
					return Diagnose(
						DiagnosticSeverity::kError,
						"$.pages",
						"expected an array");
				}

				for (size_t index = 0; index < pages->size(); ++index)
				{
					const auto location = "$.pages[" + std::to_string(index) + "]";
					const auto& value = (*pages)[index];
					if (!value.is_object())
					{
						Diagnose(
							DiagnosticSeverity::kError,
							location,
							"expected a page object");
						continue;
					}

					Page page;
					page.location = location;
					if (const auto displayName = ReadString(
							value, "pageDisplayName", location))
						page.displayName = *displayName;
					else if (const auto displayName = ReadString(
								 value, "displayName", location))
						page.displayName = *displayName;
					else
					{
						page.displayName =
							"Page " + std::to_string(index + 1);
						Diagnose(
							DiagnosticSeverity::kWarning,
							location,
							"page has no display name");
					}

					auto candidate = ReadString(value, "id", location)
						.value_or(detail::MakeIdentifier(
							page.displayName,
							"page-" + std::to_string(index + 1)));
					page.id = UniquePageId(std::move(candidate), location);

					const auto content = value.find("content");
					if (content == value.end())
					{
						Diagnose(
							DiagnosticSeverity::kError,
							location + ".content",
							"missing required content array");
					}
					else
					{
						ReadContent(*content, location + ".content", page);
					}
					a_configuration.pages.push_back(std::move(page));
				}
			}

			void ReadContent(
				const Json& a_content,
				const std::string& a_location,
				Page& a_page)
			{
				if (!a_content.is_array())
				{
					return Diagnose(
						DiagnosticSeverity::kError,
						a_location,
						"expected an array of controls");
				}
				if (a_content.empty())
				{
					Diagnose(
						DiagnosticSeverity::kWarning,
						a_location,
						"page content is empty");
				}
				for (size_t index = 0; index < a_content.size(); ++index)
				{
					const auto location =
						a_location + "[" + std::to_string(index) + "]";
					const auto& value = a_content[index];
					if (!value.is_object())
					{
						Diagnose(
							DiagnosticSeverity::kError,
							location,
							"expected a control object");
						continue;
					}
					a_page.controls.push_back(
						ReadControl(value, location, index));
				}
			}

			[[nodiscard]] Control ReadControl(
				const Json& a_value,
				const std::string& a_location,
				size_t a_index)
			{
				Control control;
				control.location = a_location;
				control.sourceIndex = a_index;
				if (const auto id = ReadString(a_value, "id", a_location))
					control.id = *id;
				if (const auto text = ReadString(a_value, "text", a_location))
					control.text = *text;
				if (const auto help = ReadString(a_value, "help", a_location))
					control.help = *help;
				if (const auto type = ReadString(
						a_value, "type", a_location, true))
				{
					control.rawType = *type;
					control.type = ResolveControlType(*type);
				}
				if (const auto condition = a_value.find("groupCondition");
					condition != a_value.end())
				{
					control.groupCondition = ReadCondition(
						*condition,
						a_location + ".groupCondition");
				}
				control.groupControl =
					ReadInteger(a_value, "groupControl", a_location);
				control.html = ReadBoolean(a_value, "html", a_location);
				control.alignment = ReadString(a_value, "align", a_location);
				if (const auto action = a_value.find("action");
					action != a_value.end())
					control.action = ReadAction(*action, a_location + ".action");
				if (control.type == ControlType::kImage)
				{
					const auto library =
						ReadString(a_value, "libName", a_location);
					const auto symbol =
						ReadString(a_value, "className", a_location);
					if (library && symbol)
					{
						control.image = Image{
							*library,
							*symbol
						};
					}
				}

				const auto valueOptions = a_value.find("valueOptions");
				if (valueOptions == a_value.end())
				{
					if (NeedsValueOptions(control.type))
					{
						Diagnose(
							control.groupControl ?
								DiagnosticSeverity::kWarning :
								DiagnosticSeverity::kError,
							a_location + ".valueOptions",
							control.groupControl ?
								"group control has no persistent value source" :
								"setting control is missing valueOptions");
					}
				}
				else if (!valueOptions->is_object())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						a_location + ".valueOptions",
						"expected an object");
				}
				else
				{
					control.valueOptions =
						ReadValueOptions(*valueOptions, control);
				}

				if (control.type == ControlType::kSlider)
				CheckSlider(control);
				if (control.type == ControlType::kStepper ||
					control.type == ControlType::kMenu)
					CheckChoice(control);

				return control;
			}

			[[nodiscard]] ValueOptions ReadValueOptions(
				const Json& a_value,
				const Control& a_control)
			{
				const auto location = a_control.location + ".valueOptions";
				ValueOptions result;
				if (const auto sourceType =
						ReadString(a_value, "sourceType", location))
					result.sourceType = ResolveSourceType(*sourceType);
				result.sourceForm =
					ReadString(a_value, "sourceForm", location);
				result.scriptName =
					ReadString(a_value, "scriptName", location);
				result.propertyName =
					ReadString(a_value, "propertyName", location);
				result.minimum = ReadNumber(a_value, "min", location);
				result.maximum = ReadNumber(a_value, "max", location);
				result.step = ReadNumber(a_value, "step", location);
				result.format = ReadString(a_value, "format", location);
				if (!result.format)
				{
					result.format =
						ReadString(a_value, "formatString", location);
				}

				if (const auto member = a_value.find("default");
					member != a_value.end())
				{
					result.defaultValue =
						ReadScalar(*member, location + ".default");
				}

				if (const auto options = a_value.find("options");
					options != a_value.end())
				{
					if (!options->is_array())
					{
						Diagnose(
							DiagnosticSeverity::kError,
							location + ".options",
							"expected an array");
					}
					else
					{
						for (size_t index = 0; index < options->size(); ++index)
						{
							if (auto option = ReadScalar(
									(*options)[index],
									location + ".options[" +
										std::to_string(index) + "]"))
								result.options.push_back(std::move(*option));
						}
					}
				}

				if (result.minimum &&
					result.maximum &&
					*result.maximum < *result.minimum)
				{
					Diagnose(
						DiagnosticSeverity::kWarning,
						location,
						"maximum is less than minimum");
				}
				if (result.step && *result.step <= 0.0)
				{
					Diagnose(
						DiagnosticSeverity::kWarning,
						location + ".step",
						"step must be greater than zero");
				}

				if (result.sourceType &&
					result.sourceType->family == SourceFamily::kModSetting)
				{
					if (a_control.id.empty())
					{
						Diagnose(
							DiagnosticSeverity::kError,
							a_control.location + ".id",
							"ModSetting source requires a setting id");
					}
					else
					{
						result.modSettingId = a_control.id;
					}
				}

				if (NeedsValueOptions(a_control.type) &&
					!result.sourceType)
				{
					Diagnose(
						DiagnosticSeverity::kWarning,
						location + ".sourceType",
						"setting value source is not declared");
				}
				return result;
			}

			void CheckSlider(const Control& a_control)
			{
				if (!a_control.valueOptions ||
					(!a_control.valueOptions->minimum &&
						!a_control.valueOptions->maximum))
				{
					Diagnose(
						DiagnosticSeverity::kError,
						a_control.location + ".valueOptions",
						"slider has no numeric range");
				}
				else if (!a_control.valueOptions->minimum ||
					!a_control.valueOptions->maximum)
				{
					Diagnose(
						DiagnosticSeverity::kWarning,
						a_control.location + ".valueOptions",
						"slider range has only one bound");
				}
			}

			void CheckChoice(const Control& a_control)
			{
				if (!a_control.valueOptions ||
					a_control.valueOptions->options.empty())
				{
					Diagnose(
						DiagnosticSeverity::kError,
						a_control.location + ".valueOptions.options",
						"choice control has no options");
				}
			}

			[[nodiscard]] std::string UniquePageId(
				std::string a_candidate,
				std::string_view a_location)
			{
				if (a_candidate.empty())
					a_candidate = "page";
				return m_diagnostics.UniqueId(
					std::move(a_candidate),
					m_pageIds,
					"page",
					a_location);
			}

			std::string m_source;
			LoadResult& m_result;
			detail::Diagnostics m_diagnostics;
			std::unordered_set<std::string> m_pageIds;
		};
	}

	LoadResult ParseConfig(
		std::string_view a_json,
		std::string_view a_source) noexcept
	{
		LoadResult result;
		auto source = std::string{ "<memory>" };
		try
		{
			if (!a_source.empty())
				source.assign(a_source);
			const auto normalized = NormalizeJson(
				a_json,
				JsonNormalizationOptions{
					.invalidEscapePassThrough = true
				});
			const auto document = Json::parse(
				normalized.begin(),
				normalized.end(),
				nullptr,
				true,
				false);
			ConfigReader reader{ source, result };
			reader.Read(document);
			if (result.configuration)
			{
				detail::MapConfiguration(
					*result.configuration,
					source,
					result.pages,
					result.diagnostics);
			}
		}
		catch (const Json::parse_error& a_error)
		{
			AddTerminalDiagnostic(
				result,
				source,
				"invalid JSON: " + std::string{ a_error.what() });
		}
		catch (const std::exception& a_error)
		{
			AddTerminalDiagnostic(
				result,
				source,
				"failed to process MCM configuration: " +
					std::string{ a_error.what() });
		}
		catch (...)
		{
			AddTerminalDiagnostic(
				result,
				source,
				"failed to process MCM configuration");
		}
		return result;
	}

	LoadResult LoadConfig(const std::filesystem::path& a_path) noexcept
	{
		LoadResult result;
		auto source = std::string{ "<path>" };
		try
		{
			source = a_path.string();
			std::ifstream stream{ a_path, std::ios::binary };
			if (!stream)
			{
				AddTerminalDiagnostic(
					result,
					source,
					"could not open MCM configuration file");
				return result;
			}

			std::ostringstream buffer;
			buffer << stream.rdbuf();
			if (stream.bad())
			{
				AddTerminalDiagnostic(
					result,
					source,
					"could not read MCM configuration file");
				return result;
			}
			return ParseConfig(buffer.str(), source);
		}
		catch (const std::exception& a_error)
		{
			AddTerminalDiagnostic(
				result,
				source,
				"could not read MCM configuration file: " +
					std::string{ a_error.what() });
		}
		catch (...)
		{
			AddTerminalDiagnostic(
				result,
				source,
				"could not read MCM configuration file");
		}
		return result;
	}
}
