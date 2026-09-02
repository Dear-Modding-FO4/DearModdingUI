#include <DearModdingUI/MCM/Compatibility.h>

#include "Mapper.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace DearModdingUI::MCM
{
	namespace
	{
		using Json = nlohmann::json;

		[[nodiscard]] std::string Lower(std::string_view a_value)
		{
			std::string result;
			result.reserve(a_value.size());
			for (const auto character : a_value)
			{
				result.push_back(static_cast<char>(
					std::tolower(static_cast<unsigned char>(character))));
			}
			return result;
		}

		[[nodiscard]] bool StartsWith(
			std::string_view a_value,
			std::string_view a_prefix)
		{
			return a_value.size() >= a_prefix.size() &&
				std::equal(
					a_prefix.begin(),
					a_prefix.end(),
					a_value.begin(),
					[](char a_left, char a_right) {
						return std::tolower(static_cast<unsigned char>(a_left)) ==
							std::tolower(static_cast<unsigned char>(a_right));
					});
		}

		[[nodiscard]] ControlType ResolveControlType(std::string_view a_type)
		{
			const auto type = Lower(a_type);
			if (type == "switch" || type == "switcher")
				return ControlType::kSwitch;
			if (type == "slider")
				return ControlType::kSlider;
			if (type == "stepper")
				return ControlType::kStepper;
			if (type == "menu" || type == "enum" || type == "dropdown")
				return ControlType::kMenu;
			if (type == "input")
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
			try
			{
				a_result.diagnostics.push_back({
					DiagnosticSeverity::kError,
					std::string{ a_source },
					"$",
					std::move(a_message)
				});
			}
			catch (...)
			{}
		}

		class ConfigReader
		{
		public:
			ConfigReader(
				std::string a_source,
				LoadResult& a_result) :
				m_source(std::move(a_source)),
				m_result(a_result)
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

				if (const auto value = ReadInteger(
						a_document, "minMcmVersion", "$", true))
					configuration.minimumMcmVersion = *value;
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
				m_result.diagnostics.push_back({
					a_severity,
					m_source,
					std::move(a_location),
					std::move(a_message)
				});
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
							"missing required integer");
					}
					return std::nullopt;
				}
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

			[[nodiscard]] std::optional<GroupCondition> ReadCondition(
				const Json& a_value,
				const std::string& a_location)
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
				const auto normalized = Lower(result.rawOperator);
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
				for (size_t index = 0; index < operation->size(); ++index)
				{
					if (auto operand = ReadCondition(
							(*operation)[index],
							a_location + "." + result.rawOperator +
								"[" + std::to_string(index) + "]"))
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
				result.sourceType =
					ReadString(a_value, "sourceType", location);
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
					StartsWith(*result.sourceType, "ModSetting"))
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
				if (m_pageIds.insert(a_candidate).second)
					return a_candidate;

				auto suffix = size_t{ 2 };
				auto unique = a_candidate + "-" + std::to_string(suffix);
				while (!m_pageIds.insert(unique).second)
					unique = a_candidate + "-" + std::to_string(++suffix);
				Diagnose(
					DiagnosticSeverity::kWarning,
					std::string{ a_location },
					"duplicate page id '" + a_candidate +
						"' was renamed to '" + unique + "'");
				return unique;
			}

			std::string m_source;
			LoadResult& m_result;
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
			const auto document = Json::parse(
				a_json.begin(),
				a_json.end(),
				nullptr,
				true,
				true);
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
