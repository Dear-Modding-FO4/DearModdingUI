#include "Mapper.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iterator>
#include <limits>
#include <type_traits>
#include <unordered_set>

namespace DearModdingUI::MCM::detail
{
	[[nodiscard]] std::string MakeIdentifier(
		std::string_view a_text,
		std::string_view a_fallback)
	{
		std::string result;
		result.reserve(a_text.size());
		auto separator = false;
		for (const auto character : a_text)
		{
			const auto value = static_cast<unsigned char>(character);
			if ((value >= 'a' && value <= 'z') ||
				(value >= 'A' && value <= 'Z') ||
				(value >= '0' && value <= '9'))
			{
				if (separator && !result.empty())
					result.push_back('-');
				result.push_back(
					value >= 'A' && value <= 'Z' ?
						static_cast<char>(value - 'A' + 'a') :
						static_cast<char>(value));
				separator = false;
			}
			else
			{
				separator = true;
			}
		}
		return result.empty() ? std::string{ a_fallback } : result;
	}

	namespace
	{
		[[nodiscard]] bool EndsWith(
			std::string_view a_value,
			std::string_view a_suffix) noexcept
		{
			if (a_value.size() < a_suffix.size())
				return false;
			const auto offset = a_value.size() - a_suffix.size();
			for (size_t index = 0; index < a_suffix.size(); ++index)
			{
				auto left = static_cast<unsigned char>(a_value[offset + index]);
				auto right = static_cast<unsigned char>(a_suffix[index]);
				if (left >= 'A' && left <= 'Z')
					left = static_cast<unsigned char>(left - 'A' + 'a');
				if (right >= 'A' && right <= 'Z')
					right = static_cast<unsigned char>(right - 'A' + 'a');
				if (left != right)
					return false;
			}
			return true;
		}

		void Diagnose(
			std::vector<Diagnostic>& a_diagnostics,
			std::string_view a_source,
			DiagnosticSeverity a_severity,
			std::string_view a_location,
			std::string a_message)
		{
			a_diagnostics.push_back({
				a_severity,
				std::string{ a_source },
				std::string{ a_location },
				std::move(a_message)
			});
		}

		[[nodiscard]] std::string ScalarText(const Scalar& a_value)
		{
			return std::visit(
				[](const auto& a_scalar) -> std::string {
					using T = std::remove_cvref_t<decltype(a_scalar)>;
					if constexpr (std::same_as<T, bool>)
					{
						return a_scalar ? "true" : "false";
					}
					else if constexpr (std::same_as<T, std::string>)
					{
						return a_scalar;
					}
					else if constexpr (
						std::same_as<T, int64_t> ||
						std::same_as<T, uint64_t>)
					{
						return std::to_string(a_scalar);
					}
					else
					{
						char buffer[64]{};
						const auto converted = std::to_chars(
							std::begin(buffer),
							std::end(buffer),
							a_scalar,
							std::chars_format::general);
						return converted.ec == std::errc{} ?
							std::string{ buffer, converted.ptr } :
							std::to_string(a_scalar);
					}
				},
				a_value);
		}

		[[nodiscard]] std::optional<double> DoubleValue(
			const Scalar& a_value) noexcept
		{
			return std::visit(
				[](const auto& a_scalar) -> std::optional<double> {
					using T = std::remove_cvref_t<decltype(a_scalar)>;
					if constexpr (
						std::same_as<T, int64_t> ||
						std::same_as<T, uint64_t> ||
						std::same_as<T, double>)
						return static_cast<double>(a_scalar);
					else
						return std::nullopt;
				},
				a_value);
		}

		[[nodiscard]] std::optional<int64_t> SignedValue(
			const Scalar& a_value) noexcept
		{
			return std::visit(
				[](const auto& a_scalar) -> std::optional<int64_t> {
					using T = std::remove_cvref_t<decltype(a_scalar)>;
					if constexpr (std::same_as<T, int64_t>)
					{
						return a_scalar;
					}
					else if constexpr (std::same_as<T, uint64_t>)
					{
						if (a_scalar <= static_cast<uint64_t>(
								(std::numeric_limits<int64_t>::max)()))
							return static_cast<int64_t>(a_scalar);
						return std::nullopt;
					}
					else if constexpr (std::same_as<T, double>)
					{
						const auto value = static_cast<long double>(a_scalar);
						if (std::isfinite(a_scalar) &&
							std::trunc(a_scalar) == a_scalar &&
							value >= static_cast<long double>(
								(std::numeric_limits<int64_t>::min)()) &&
							value <= static_cast<long double>(
								(std::numeric_limits<int64_t>::max)()))
							return static_cast<int64_t>(a_scalar);
						return std::nullopt;
					}
					else
					{
						return std::nullopt;
					}
				},
				a_value);
		}

		[[nodiscard]] std::optional<int64_t> SignedBound(
			const std::optional<double>& a_value) noexcept
		{
			const auto value = a_value ?
				static_cast<long double>(*a_value) :
				0.0L;
			if (!a_value ||
				!std::isfinite(*a_value) ||
				std::trunc(*a_value) != *a_value ||
				value < static_cast<long double>(
					(std::numeric_limits<int64_t>::min)()) ||
				value > static_cast<long double>(
					(std::numeric_limits<int64_t>::max)()))
				return std::nullopt;
			return static_cast<int64_t>(*a_value);
		}

		[[nodiscard]] float DragSpeed(
			const std::optional<double>& a_step) noexcept
		{
			if (!a_step ||
				!std::isfinite(*a_step) ||
				*a_step <= 0.0 ||
				*a_step > static_cast<double>(
					(std::numeric_limits<float>::max)()))
				return 0.0f;
			return static_cast<float>(*a_step);
		}

		[[nodiscard]] bool UsesSignedNumbers(const Control& a_control)
		{
			return a_control.valueOptions &&
				a_control.valueOptions->sourceType &&
				EndsWith(*a_control.valueOptions->sourceType, "Int");
		}

		[[nodiscard]] bool UsesStringChoices(const Control& a_control)
		{
			return a_control.valueOptions &&
				a_control.valueOptions->sourceType &&
				EndsWith(*a_control.valueOptions->sourceType, "String");
		}

		void MapCheckboxDefault(
			const Control& a_control,
			dmui::SettingDescriptor& a_descriptor,
			std::string_view a_source,
			std::vector<Diagnostic>& a_diagnostics)
		{
			auto value = false;
			if (a_control.valueOptions &&
				a_control.valueOptions->defaultValue)
			{
				const auto& declared =
					*a_control.valueOptions->defaultValue;
				if (const auto boolean = std::get_if<bool>(&declared))
				{
					value = *boolean;
				}
				else if (const auto number = DoubleValue(declared);
					number && (*number == 0.0 || *number == 1.0))
				{
					value = *number != 0.0;
				}
				else
				{
					Diagnose(
						a_diagnostics,
						a_source,
						DiagnosticSeverity::kWarning,
						a_control.location + ".valueOptions.default",
						"checkbox default is not boolean");
				}
			}
			a_descriptor.defaultValue = value;
		}

		void MapDoubleControl(
			const Control& a_control,
			dmui::SettingDescriptor& a_descriptor,
			std::string_view a_source,
			std::vector<Diagnostic>& a_diagnostics)
		{
			dmui::DoubleSettingControl mapped;
			if (a_control.valueOptions)
			{
				const auto& options = *a_control.valueOptions;
				if (options.minimum || options.maximum)
				{
					mapped.range = dmui::NumericSettingRange<double>{
						options.minimum,
						options.maximum
					};
				}
				if (options.format)
					mapped.format = *options.format;
				mapped.dragSpeed = DragSpeed(options.step);
				if (options.defaultValue)
				{
					if (const auto value = DoubleValue(*options.defaultValue))
						a_descriptor.defaultValue = *value;
					else
					{
						Diagnose(
							a_diagnostics,
							a_source,
							DiagnosticSeverity::kWarning,
							a_control.location + ".valueOptions.default",
							"numeric default is not a number");
						a_descriptor.defaultValue = 0.0;
					}
				}
				else
				{
					a_descriptor.defaultValue = 0.0;
				}
			}
			else
			{
				a_descriptor.defaultValue = 0.0;
			}
			a_descriptor.control = std::move(mapped);
		}

		void MapSignedControl(
			const Control& a_control,
			dmui::SettingDescriptor& a_descriptor,
			std::string_view a_source,
			std::vector<Diagnostic>& a_diagnostics)
		{
			dmui::SignedSettingControl mapped;
			if (a_control.valueOptions)
			{
				const auto& options = *a_control.valueOptions;
				const auto minimum = SignedBound(options.minimum);
				const auto maximum = SignedBound(options.maximum);
				if (minimum || maximum)
				{
					mapped.range = dmui::NumericSettingRange<int64_t>{
						minimum,
						maximum
					};
				}
				if (options.minimum && !minimum)
				{
					Diagnose(
						a_diagnostics,
						a_source,
						DiagnosticSeverity::kWarning,
						a_control.location + ".valueOptions.min",
						"integer setting has a non-integral or out-of-range minimum");
				}
				if (options.maximum && !maximum)
				{
					Diagnose(
						a_diagnostics,
						a_source,
						DiagnosticSeverity::kWarning,
						a_control.location + ".valueOptions.max",
						"integer setting has a non-integral or out-of-range maximum");
				}
				if (options.format)
					mapped.format = *options.format;
				mapped.dragSpeed = DragSpeed(options.step);
				if (options.defaultValue)
				{
					if (const auto value = SignedValue(*options.defaultValue))
						a_descriptor.defaultValue = *value;
					else
					{
						Diagnose(
							a_diagnostics,
							a_source,
							DiagnosticSeverity::kWarning,
							a_control.location + ".valueOptions.default",
							"integer default is not an integral 64-bit value");
						a_descriptor.defaultValue = int64_t{};
					}
				}
				else
				{
					a_descriptor.defaultValue = int64_t{};
				}
			}
			else
			{
				a_descriptor.defaultValue = int64_t{};
			}
			a_descriptor.control = std::move(mapped);
		}

		void MapChoiceControl(
			const Control& a_control,
			dmui::SettingDescriptor& a_descriptor)
		{
			dmui::ChoiceSettingControl mapped;
			const auto stringChoices = UsesStringChoices(a_control);
			if (a_control.valueOptions)
			{
				const auto& options = *a_control.valueOptions;
				mapped.options.reserve(options.options.size());
				for (size_t index = 0; index < options.options.size(); ++index)
				{
					auto label = ScalarText(options.options[index]);
					mapped.options.push_back({
						stringChoices ?
							label :
							std::to_string(index),
						std::move(label)
					});
				}
				if (options.defaultValue)
				{
					a_descriptor.defaultValue =
						ScalarText(*options.defaultValue);
				}
				else if (!mapped.options.empty())
				{
					a_descriptor.defaultValue =
						mapped.options.front().value;
				}
				else
				{
					a_descriptor.defaultValue = std::string{};
				}
			}
			else
			{
				a_descriptor.defaultValue = std::string{};
			}
			a_descriptor.control = std::move(mapped);
		}

		[[nodiscard]] dmui::SettingDescriptor MapControl(
			const Control& a_control,
			std::string a_id,
			std::string_view a_source,
			std::vector<Diagnostic>& a_diagnostics)
		{
			dmui::SettingDescriptor descriptor;
			descriptor.id = std::move(a_id);
			descriptor.label = a_control.text.empty() ?
				(a_control.id.empty() ? a_control.rawType : a_control.id) :
				a_control.text;
			if (descriptor.label.empty())
				descriptor.label = descriptor.id;
			descriptor.description = a_control.help;

			switch (a_control.type)
			{
			case ControlType::kSwitch:
				descriptor.control = dmui::CheckboxSettingControl{};
				MapCheckboxDefault(
					a_control,
					descriptor,
					a_source,
					a_diagnostics);
				break;
			case ControlType::kSlider:
				if (UsesSignedNumbers(a_control))
				{
					MapSignedControl(
						a_control,
						descriptor,
						a_source,
						a_diagnostics);
				}
				else
				{
					MapDoubleControl(
						a_control,
						descriptor,
						a_source,
						a_diagnostics);
				}
				break;
			case ControlType::kStepper:
			case ControlType::kMenu:
				MapChoiceControl(a_control, descriptor);
				break;
			case ControlType::kInput:
			{
				auto defaultValue = std::string{};
				if (a_control.valueOptions &&
					a_control.valueOptions->defaultValue)
				{
					defaultValue =
						ScalarText(*a_control.valueOptions->defaultValue);
				}
				descriptor.control = dmui::TextSettingControl{
					(std::max)(size_t{ 512 }, defaultValue.size() + 1)
				};
				descriptor.defaultValue = std::move(defaultValue);
				break;
			}
			case ControlType::kText:
				descriptor.control = dmui::ReadOnlySettingControl{};
				descriptor.defaultValue = a_control.text;
				descriptor.showReset = false;
				break;
			default:
				descriptor.control = dmui::UnsupportedSettingControl{
					static_cast<uint32_t>(a_control.type) + 1
				};
				descriptor.defaultValue = false;
				descriptor.showReset = false;
				break;
			}
			return descriptor;
		}

		[[nodiscard]] std::string UniqueId(
			std::string a_candidate,
			std::unordered_set<std::string>& a_ids,
			std::string_view a_source,
			std::string_view a_location,
			std::string_view a_kind,
			std::vector<Diagnostic>& a_diagnostics)
		{
			if (a_ids.insert(a_candidate).second)
				return a_candidate;
			auto suffix = size_t{ 2 };
			auto unique = a_candidate + "-" + std::to_string(suffix);
			while (!a_ids.insert(unique).second)
				unique = a_candidate + "-" + std::to_string(++suffix);
			Diagnose(
				a_diagnostics,
				a_source,
				DiagnosticSeverity::kWarning,
				a_location,
				"duplicate " + std::string{ a_kind } + " id '" +
					a_candidate + "' was renamed to '" + unique + "'");
			return unique;
		}

		void DiagnoseUnsupported(
			const Control& a_control,
			std::string_view a_source,
			std::vector<Diagnostic>& a_diagnostics)
		{
			if (a_control.type == ControlType::kUnknown)
			{
				Diagnose(
					a_diagnostics,
					a_source,
					DiagnosticSeverity::kWarning,
					a_control.location,
					a_control.rawType.empty() ?
						"control with no type maps to unsupported" :
						"unknown MCM control type '" + a_control.rawType +
							"' maps to unsupported");
				return;
			}
			switch (a_control.type)
			{
			case ControlType::kButton:
			case ControlType::kKeymap:
			case ControlType::kColor:
			case ControlType::kImage:
				Diagnose(
					a_diagnostics,
					a_source,
					DiagnosticSeverity::kWarning,
					a_control.location,
					"MCM control type '" + a_control.rawType +
						"' is unsupported in this phase");
				break;
			default:
				break;
			}
		}

		[[nodiscard]] MappedPage MapPage(
			const Page& a_page,
			std::string_view a_source,
			std::vector<Diagnostic>& a_diagnostics)
		{
			MappedPage mapped;
			mapped.id = a_page.id;
			mapped.displayName = a_page.displayName;

			std::unordered_set<std::string> groupIds;
			std::unordered_set<std::string> descriptorIds;
			std::optional<size_t> currentGroup;
			auto descriptorCount = size_t{};

			const auto addGroup = [&](
				std::string a_id,
				std::string a_label,
				std::string_view a_location) {
				a_id = UniqueId(
					std::move(a_id),
					groupIds,
					a_source,
					a_location,
					"group",
					a_diagnostics);
				mapped.settings.groups.push_back({
					std::move(a_id),
					std::move(a_label)
				});
				currentGroup = mapped.settings.groups.size() - 1;
			};

			for (const auto& control : a_page.controls)
			{
				if (control.type == ControlType::kGroup)
				{
					auto label = control.text.empty() ?
						"Section" :
						control.text;
					auto id = control.id.empty() ?
						MakeIdentifier(label, "section") + "-" +
							std::to_string(control.sourceIndex + 1) :
						control.id;
					addGroup(
						std::move(id),
						std::move(label),
						control.location);
					continue;
				}
				if (control.type == ControlType::kSpacing ||
					control.type == ControlType::kHidden)
					continue;

				if (!currentGroup)
					addGroup("general", "General", a_page.location);

				auto id = control.id.empty() ?
					"control-" + std::to_string(control.sourceIndex + 1) :
					control.id;
				id = UniqueId(
					std::move(id),
					descriptorIds,
					a_source,
					control.location,
					"setting",
					a_diagnostics);
				mapped.settings.groups[*currentGroup].settings.push_back(
					MapControl(
						control,
						std::move(id),
						a_source,
						a_diagnostics));
				++descriptorCount;

				DiagnoseUnsupported(
					control,
					a_source,
					a_diagnostics);
				if (control.html.value_or(false))
				{
					Diagnose(
						a_diagnostics,
						a_source,
						DiagnosticSeverity::kWarning,
						control.location + ".html",
						"HTML text presentation is not represented by settings controls");
				}
				if (control.alignment)
				{
					Diagnose(
						a_diagnostics,
						a_source,
						DiagnosticSeverity::kWarning,
						control.location + ".align",
						"text alignment is not represented by settings controls");
				}
			}

			if (descriptorCount == 0)
			{
				Diagnose(
					a_diagnostics,
					a_source,
					DiagnosticSeverity::kWarning,
					a_page.location,
					"page produced no setting descriptors");
			}
			return mapped;
		}
	}

	void MapConfiguration(
		const Configuration& a_configuration,
		std::string_view a_source,
		std::vector<MappedPage>& a_pages,
		std::vector<Diagnostic>& a_diagnostics)
	{
		a_pages.reserve(a_pages.size() + a_configuration.pages.size());
		for (const auto& page : a_configuration.pages)
		{
			a_pages.push_back(MapPage(
				page,
				a_source,
				a_diagnostics));
		}
	}
}
