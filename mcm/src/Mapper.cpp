#include "Mapper.h"

#include "Conditions.h"
#include "Diagnostics.h"
#include "SliderNormalization.h"

#include <DearModdingUI/MCM/FileChoices.h>
#include <DearModdingUI/MCM/SettingsIni.h>
#include <DearModdingUI/MCM/ValueSource.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iterator>
#include <limits>
#include <stdexcept>
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
		class DisplayTextResolver
		{
		public:
			DisplayTextResolver(
				const TextResolver& a_resolver,
				detail::Diagnostics& a_diagnostics) :
				resolver_(a_resolver),
				diagnostics_(a_diagnostics)
			{}

			[[nodiscard]] std::string Resolve(
				std::string_view a_text,
				std::string_view a_location,
				bool a_html = false)
			{
				if (!resolver_ || failed_)
					return std::string{ a_text };
				if (a_html)
					return ResolveHtml(a_text, a_location);
				if (a_text.size() < 2 || a_text.front() != '$')
					return std::string{ a_text };
				try
				{
					if (auto value = resolver_(a_text))
						return std::move(*value);
				}
				catch (const std::exception& a_error)
				{
					DiagnoseFailure(a_location, a_error.what());
					return std::string{ a_text };
				}
				catch (...)
				{
					DiagnoseFailure(a_location, {});
					return std::string{ a_text };
				}

				if (missingKeys_.insert(std::string{ a_text }).second)
				{
					if (missingKeys_.size() <= kMaxMissingDiagnostics)
						diagnostics_.Add(
							DiagnosticSeverity::kWarning,
							std::string{ a_location },
							"localization key not found: " + std::string{ a_text });
					else if (missingKeys_.size() == kMaxMissingDiagnostics + 1)
						diagnostics_.Add(
							DiagnosticSeverity::kWarning,
							std::string{ a_location },
							"additional missing localization keys omitted");
				}
				return std::string{ a_text };
			}

		private:
			static constexpr size_t kMaxMissingDiagnostics = 32;

			[[nodiscard]] std::string ResolveHtml(
				std::string_view a_text,
				std::string_view a_location)
			{
				std::string result;
				result.reserve(a_text.size());
				auto offset = size_t{};
				while (offset < a_text.size())
				{
					const auto tag = a_text.find('<', offset);
					const auto textEnd =
						tag == std::string_view::npos ? a_text.size() : tag;
					AppendTextNode(
						result,
						a_text.substr(offset, textEnd - offset),
						a_location);
					if (tag == std::string_view::npos)
						break;
					const auto tagEnd = a_text.find('>', tag + 1);
					if (tagEnd == std::string_view::npos)
					{
						result.append(a_text.substr(tag));
						break;
					}
					result.append(a_text.substr(tag, tagEnd - tag + 1));
					offset = tagEnd + 1;
				}
				return result;
			}

			void AppendTextNode(
				std::string& a_result,
				std::string_view a_text,
				std::string_view a_location)
			{
				const auto begin = a_text.find_first_not_of(" \t\r\n");
				if (begin == std::string_view::npos)
				{
					a_result.append(a_text);
					return;
				}
				const auto end = a_text.find_last_not_of(" \t\r\n") + 1;
				a_result.append(a_text.substr(0, begin));
				a_result.append(Resolve(a_text.substr(begin, end - begin), a_location));
				a_result.append(a_text.substr(end));
			}

			void DiagnoseFailure(
				std::string_view a_location,
				std::string_view a_detail)
			{
				if (failed_)
					return;
				failed_ = true;
				auto message =
					std::string{ "text resolver failed; localization keys were preserved" };
				if (!a_detail.empty())
					message += ": " + std::string{ a_detail };
				diagnostics_.Add(
					DiagnosticSeverity::kError,
					std::string{ a_location },
					std::move(message));
			}

			const TextResolver& resolver_;
			detail::Diagnostics& diagnostics_;
			std::unordered_set<std::string> missingKeys_;
			bool failed_{};
		};

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
						if (std::isfinite(a_scalar) &&
							std::trunc(a_scalar) == a_scalar &&
							a_scalar >= -0x1p63 &&
							a_scalar < 0x1p63)
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
			if (!a_value ||
				!std::isfinite(*a_value) ||
				std::trunc(*a_value) != *a_value ||
				*a_value < -0x1p63 ||
				*a_value >= 0x1p63)
				return std::nullopt;
			return static_cast<int64_t>(*a_value);
		}

		[[nodiscard]] bool UsesSignedNumbers(const Control& a_control)
		{
			return a_control.valueOptions &&
				a_control.valueOptions->sourceType &&
				a_control.valueOptions->sourceType->value ==
					SourceValueKind::kInt;
		}

		[[nodiscard]] bool UsesStringChoices(const Control& a_control)
		{
			return a_control.valueOptions &&
				a_control.valueOptions->sourceType &&
				a_control.valueOptions->sourceType->value ==
					SourceValueKind::kString;
		}

		[[nodiscard]] bool HasValidIntegerSliderParameters(
			const Control& a_control) noexcept
		{
			if (a_control.type != ControlType::kSlider ||
				!UsesSignedNumbers(a_control) ||
				!a_control.valueOptions)
				return true;
			const auto& options = *a_control.valueOptions;
			if (options.sliderDefaultsApplied)
				return true;
			const auto minimum = SignedBound(options.minimum);
			const auto maximum = SignedBound(options.maximum);
			const auto step = SignedBound(options.step);
			return minimum && maximum && step && *step > 0;
		}

		void MapCheckboxDefault(
			const Control& a_control,
			dmui::SettingDescriptor& a_descriptor,
			detail::Diagnostics& a_diag)
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
					a_diag.Add(
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
			detail::Diagnostics& a_diag)
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
				if (options.step &&
					std::isfinite(*options.step) &&
					*options.step > 0.0 &&
					(!options.minimum || std::isfinite(*options.minimum)))
				{
					mapped.quantization = dmui::NumericQuantization<double>{
						*options.step,
						a_control.type == ControlType::kSlider ?
							0.0 :
							options.minimum.value_or(0.0)
					};
				}
				else if (options.step)
				{
					a_diag.Add(
						DiagnosticSeverity::kWarning,
						a_control.location + ".valueOptions.step",
						"numeric setting has a non-positive or non-finite step or origin");
				}
				if (options.defaultValue)
				{
					if (const auto value = DoubleValue(*options.defaultValue))
						a_descriptor.defaultValue = *value;
					else
					{
						a_diag.Add(
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
			detail::Diagnostics& a_diag)
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
					a_diag.Add(
						DiagnosticSeverity::kWarning,
						a_control.location + ".valueOptions.min",
						"integer setting has a non-integral or out-of-range minimum");
				}
				if (options.maximum && !maximum)
				{
					a_diag.Add(
						DiagnosticSeverity::kWarning,
						a_control.location + ".valueOptions.max",
						"integer setting has a non-integral or out-of-range maximum");
				}
				if (options.format)
					mapped.format = *options.format;
				auto step = SignedBound(options.step);
				if (a_control.type == ControlType::kSlider &&
					options.sliderDefaultsApplied)
					step = int64_t{ 1 };
				if (step && *step > 0 && (!options.minimum || minimum))
				{
					mapped.quantization = dmui::NumericQuantization<int64_t>{
						*step,
						a_control.type == ControlType::kSlider ?
							0 :
							minimum.value_or(0)
					};
				}
				else if (options.step)
				{
					a_diag.Add(
						DiagnosticSeverity::kWarning,
						a_control.location + ".valueOptions.step",
						"integer setting has an invalid step or quantization origin");
				}
				if (options.defaultValue)
				{
					if (const auto value = SignedValue(*options.defaultValue))
						a_descriptor.defaultValue = *value;
					else
					{
						a_diag.Add(
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
			dmui::SettingDescriptor& a_descriptor,
			DisplayTextResolver& a_textResolver)
		{
			dmui::ChoiceSettingControl mapped;
			const auto stringChoices = UsesStringChoices(a_control);
			if (a_control.valueOptions)
			{
				const auto& options = *a_control.valueOptions;
				mapped.options.reserve(options.options.size());
				for (size_t index = 0; index < options.options.size(); ++index)
				{
					auto rawLabel = ScalarText(options.options[index]);
					auto label = a_textResolver.Resolve(
						rawLabel,
						a_control.location + ".valueOptions.options[" +
							std::to_string(index) + "]");
					mapped.options.push_back({
						stringChoices ?
							rawLabel :
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

		void MapFileChoiceControl(dmui::SettingDescriptor& a_descriptor)
		{
			dmui::ChoiceSettingControl mapped;
			mapped.options.push_back({ "", "None" });
			mapped.unmatchedLabel = "None";
			a_descriptor.defaultValue = std::string{};
			a_descriptor.control = std::move(mapped);
		}

		void ExtractSliderNormalization(
			dmui::SettingDescriptor& a_descriptor,
			MappedRow& a_row)
		{
			if (auto* control =
					std::get_if<dmui::DoubleSettingControl>(
						&a_descriptor.control);
				control &&
				control->range &&
				control->range->minimum &&
				control->range->maximum &&
				control->quantization)
			{
				a_row.sliderNormalization = DoubleSliderNormalization{
					*control->range->minimum,
					*control->range->maximum,
					control->quantization->interval
				};
				control->quantization.reset();
				a_descriptor.defaultValue = NormalizeSliderValue(
					*a_row.sliderNormalization, a_descriptor.defaultValue);
				return;
			}
			if (auto* control =
					std::get_if<dmui::SignedSettingControl>(
						&a_descriptor.control);
				control &&
				control->range &&
				control->range->minimum &&
				control->range->maximum &&
				control->quantization)
			{
				a_row.sliderNormalization = SignedSliderNormalization{
					*control->range->minimum,
					*control->range->maximum,
					control->quantization->interval
				};
				control->quantization.reset();
				a_descriptor.defaultValue = NormalizeSliderValue(
					*a_row.sliderNormalization, a_descriptor.defaultValue);
			}
		}

		[[nodiscard]] dmui::SettingDescriptor MapControl(
			const Control& a_control,
			std::string a_id,
			MappedRow& a_row,
			detail::Diagnostics& a_diag,
			DisplayTextResolver& a_textResolver)
		{
			dmui::SettingDescriptor descriptor;
			descriptor.id = std::move(a_id);
			descriptor.label = a_control.text.empty() &&
					a_control.type != ControlType::kText ?
				(a_control.id.empty() ? a_control.rawType : a_control.id) :
				a_textResolver.Resolve(
					a_control.text,
					a_control.location + ".text",
					a_control.html.value_or(false));
			if (descriptor.label.empty() && a_control.type != ControlType::kText)
				descriptor.label = descriptor.id;
			descriptor.description = a_textResolver.Resolve(
				a_control.help,
				a_control.location + ".help");

			switch (a_control.type)
			{
			case ControlType::kSwitch:
				descriptor.control = dmui::CheckboxSettingControl{};
				MapCheckboxDefault(
					a_control,
					descriptor,
					a_diag);
				break;
			case ControlType::kHidden:
			{
				const auto kind =
					a_control.valueOptions &&
						a_control.valueOptions->sourceType ?
					a_control.valueOptions->sourceType->value :
					SourceValueKind::kNone;
				switch (kind)
				{
				case SourceValueKind::kBool:
					descriptor.control = dmui::CheckboxSettingControl{};
					MapCheckboxDefault(a_control, descriptor, a_diag);
					break;
				case SourceValueKind::kInt:
					MapSignedControl(a_control, descriptor, a_diag);
					break;
				case SourceValueKind::kString:
					descriptor.control = dmui::TextSettingControl{};
					descriptor.defaultValue = a_control.valueOptions &&
							a_control.valueOptions->defaultValue ?
						ScalarText(*a_control.valueOptions->defaultValue) :
						std::string{};
					break;
				case SourceValueKind::kFloat:
				case SourceValueKind::kNone:
					MapDoubleControl(a_control, descriptor, a_diag);
					break;
				}
				break;
			}
			case ControlType::kSlider:
				if (UsesSignedNumbers(a_control))
				{
					MapSignedControl(
						a_control,
						descriptor,
						a_diag);
				}
				else
				{
					MapDoubleControl(
						a_control,
						descriptor,
						a_diag);
				}
				if (a_control.valueOptions &&
					a_control.valueOptions->sliderParametersValid &&
					HasValidIntegerSliderParameters(a_control))
					ExtractSliderNormalization(descriptor, a_row);
				break;
			case ControlType::kStepper:
			case ControlType::kMenu:
				MapChoiceControl(a_control, descriptor, a_textResolver);
				break;
			case ControlType::kFileMenu:
				MapFileChoiceControl(descriptor);
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
			{
				const auto alignment = a_control.alignment ?
					std::optional<std::string_view>{ *a_control.alignment } :
					std::nullopt;
				auto presentation = ResolveTextPresentation(
					descriptor.label,
					a_control.html.value_or(false),
					alignment);
				descriptor.label.clear();
				descriptor.defaultValue = presentation.text;
				descriptor.control = dmui::ReadOnlySettingControl{};
				a_row.text = MappedText{
					descriptor.id,
					std::move(presentation)
				};
				descriptor.showReset = false;
				descriptor.presentation = {
					dmui::RowPresentation::LabelMode::kHidden,
					dmui::RowPresentation::Layout::kFullSpan
				};
				break;
			}
			case ControlType::kKeymap:
				descriptor.control = dmui::ReadOnlySettingControl{};
				descriptor.defaultValue = std::string{ "Unbound" };
				a_row.text = MappedText{
					descriptor.id,
					TextPresentation{ "Unbound" }
				};
				a_row.keybindId = a_control.id;
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

		[[nodiscard]] std::optional<MappedBinding> MapBinding(
			const Control& a_control,
			std::string_view a_id,
			const dmui::SettingValue& a_target,
			detail::Diagnostics& a_diag)
		{
			if (!a_control.valueOptions ||
				!a_control.valueOptions->sourceType)
				return std::nullopt;
			const auto& options = *a_control.valueOptions;
			const auto& type = *options.sourceType;
			const auto location = a_control.location + ".valueOptions";
			if (a_control.type == ControlType::kFileMenu &&
				type.value != SourceValueKind::kString)
				return std::nullopt;
			switch (type.family)
			{
			case SourceFamily::kGlobal:
			{
				if (!options.sourceForm || options.sourceForm->empty())
				{
					a_diag.Add(
						DiagnosticSeverity::kError,
						location + ".sourceForm",
						"GlobalValue source requires a form identifier");
					return std::nullopt;
				}
				auto result = MappedBinding{
					std::string{ a_id },
					a_target,
					type.value,
					type.raw,
					GlobalBinding{ *options.sourceForm }
				};
				result.cacheKey = MakeBindingKey(result);
				return result;
			}
			case SourceFamily::kProperty:
			{
				if (!options.sourceForm || options.sourceForm->empty() ||
					!options.propertyName || options.propertyName->empty())
				{
					a_diag.Add(
						DiagnosticSeverity::kError,
						location,
						"PropertyValue source requires form and propertyName");
					return std::nullopt;
				}
				auto result = MappedBinding{
					std::string{ a_id },
					a_target,
					type.value,
					type.raw,
					PropertyBinding{
						*options.sourceForm,
						options.scriptName,
						*options.propertyName
					}
				};
				result.cacheKey = MakeBindingKey(result);
				return result;
			}
			case SourceFamily::kModSetting:
			{
				const auto setting = options.modSettingId ?
					ParseSettingIdentifier(*options.modSettingId) :
					std::nullopt;
				if (!setting)
				{
					a_diag.Add(
						DiagnosticSeverity::kError,
						a_control.location + ".id",
						"ModSetting source requires a valid setting id");
					return std::nullopt;
				}
				auto result = MappedBinding{
					std::string{ a_id },
					a_target,
					type.value,
					type.raw,
					ModSettingBinding{
						setting->section,
						setting->key
					}
				};
				result.cacheKey = MakeBindingKey(result);
				return result;
			}
			case SourceFamily::kUnknown:
				a_diag.Add(
					DiagnosticSeverity::kWarning,
					location + ".sourceType",
					"unknown setting value source '" + type.raw + "'");
				return std::nullopt;
			}
			return std::nullopt;
		}

		[[nodiscard]] bool IsIdlessLocalToggle(
			const Control& a_control,
			const std::unordered_set<int64_t>& a_referencedControls)
		{
			return a_control.id.empty() &&
				a_control.type == ControlType::kSwitch &&
				a_control.groupControl &&
				a_referencedControls.contains(*a_control.groupControl) &&
				a_control.valueOptions &&
				a_control.valueOptions->sourceType &&
				a_control.valueOptions->sourceType->family ==
					SourceFamily::kModSetting &&
				a_control.valueOptions->sourceType->value ==
					SourceValueKind::kBool;
		}

		void DiagnoseUnsupported(
			const Control& a_control,
			detail::Diagnostics& a_diag)
		{
			if (a_control.type == ControlType::kUnknown)
			{
				a_diag.Add(
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
			case ControlType::kColor:
				a_diag.Add(
					DiagnosticSeverity::kWarning,
					a_control.location,
					"MCM control type '" + a_control.rawType +
						"' is unsupported in this phase");
				break;
			case ControlType::kImage:
			{
				auto message = std::string{ "SWF component not rendered: " };
				if (a_control.image &&
					!a_control.image->library.empty() &&
					!a_control.image->symbol.empty())
				{
					message += a_control.image->library + "::" +
						a_control.image->symbol;
				}
				else
				{
					message += "missing ";
					const auto missingLibrary =
						!a_control.image ||
						a_control.image->library.empty();
					const auto missingSymbol =
						!a_control.image ||
						a_control.image->symbol.empty();
					if (missingLibrary && missingSymbol)
						message += "libName and className metadata";
					else if (missingLibrary)
						message += "libName metadata";
					else
						message += "className metadata";
				}
				a_diag.Add(
					DiagnosticSeverity::kWarning,
					a_control.location,
					std::move(message));
				break;
			}
			default:
				break;
			}
		}

		[[nodiscard]] MappedPage MapPage(
			const Page& a_page,
			std::string_view a_displayName,
			detail::Diagnostics& a_diag,
			DisplayTextResolver& a_textResolver)
		{
			MappedPage mapped;
			mapped.id = a_page.id;
			mapped.displayName = a_page.root && !a_displayName.empty() ?
				std::string{ a_displayName } :
				a_textResolver.Resolve(a_page.displayName, a_page.location);

			std::unordered_set<std::string> groupIds;
			std::unordered_set<std::string> descriptorIds;
			std::optional<size_t> currentGroup;
			auto descriptorCount = size_t{};
			const auto referencedControls =
				detail::BuildReferencedControls(a_page.controls);

			const auto addGroup = [&](
				std::string a_id,
				std::string a_label,
				std::string_view a_location) {
				a_id = a_diag.UniqueId(
					std::move(a_id),
					groupIds,
					"group",
					a_location);
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
					const auto unnamed = control.text.empty();
					if (unnamed && currentGroup)
					{
						mapped.settings.groups[*currentGroup].rows.emplace_back(
							dmui::SettingGroup::DividerRow{});
						continue;
					}
					auto rawLabel = control.text;
					auto id = control.id.empty() ?
						MakeIdentifier(rawLabel, "section") + "-" +
							std::to_string(control.sourceIndex + 1) :
						control.id;
					auto label = a_textResolver.Resolve(
						rawLabel,
						control.location + ".text",
						control.html.value_or(false));
					label = ResolveTextPresentation(
						label, control.html.value_or(false)).text;
					addGroup(
						std::move(id),
						std::move(label),
						control.location);
					if (unnamed)
					{
						mapped.settings.groups[*currentGroup].headingMode =
							dmui::SettingGroup::HeadingMode::kDivider;
					}
					continue;
				}
				if (control.type == ControlType::kSpacing)
					continue;

				if (!currentGroup)
					addGroup("general", "General", a_page.location);

				auto id = control.id.empty() ?
					"control-" + std::to_string(control.sourceIndex + 1) :
					control.id;
				id = a_diag.UniqueId(
					std::move(id),
					descriptorIds,
					"setting",
					control.location);
				MappedRow row;
				row.id = id;
				row.emitted = control.type != ControlType::kHidden &&
					control.type != ControlType::kImage;
				row.unsupported = control.type == ControlType::kImage;
				row.groupControl = control.groupControl;
				row.groupCondition = control.groupCondition;
				row.action = control.action;
				row.image = control.image;
				if (control.type == ControlType::kImage)
				{
					mapped.rows.push_back(std::move(row));
					DiagnoseUnsupported(control, a_diag);
					continue;
				}
				if (control.type == ControlType::kButton)
				{
					auto label = control.text.empty() ?
						(control.id.empty() ? "Action" : control.id) :
						a_textResolver.Resolve(
							control.text,
							control.location + ".text",
							control.html.value_or(false));
					auto description = a_textResolver.Resolve(
						control.help,
						control.location + ".help");
					if (!control.action)
					{
						if (!description.empty())
							description.push_back('\n');
						description += "This button has no action.";
					}
					auto& group = mapped.settings.groups[*currentGroup];
					dmui::SettingsActionRow action;
					action.id = id;
					action.buttonLabel = std::move(label);
					action.description = std::move(description);
					if (!control.action)
						action.isEnabled = [] { return false; };
					action.presentation = {
						dmui::RowPresentation::LabelMode::kHidden,
						dmui::RowPresentation::Layout::kFullSpan
					};
					group.actionRows.push_back(std::move(action));
					group.rows.emplace_back(
						dmui::SettingGroup::ActionIndex{
							group.actionRows.size() - 1
						});
					mapped.rows.push_back(std::move(row));
					++descriptorCount;
					continue;
				}
				auto descriptor = MapControl(
					control,
					id,
					row,
					a_diag,
					a_textResolver);
				row.unsupported =
					std::holds_alternative<dmui::UnsupportedSettingControl>(
						descriptor.control) ||
					(control.type == ControlType::kSlider &&
						control.valueOptions &&
						(!control.valueOptions->sliderParametersValid ||
							!HasValidIntegerSliderParameters(control)));
				if (control.type == ControlType::kFileMenu)
				{
					row.fileChoices = FileChoiceMetadata{
						control.valueOptions ?
							control.valueOptions->filePath :
							std::nullopt,
						control.valueOptions &&
								control.valueOptions->fileMask ?
							*control.valueOptions->fileMask :
							"*",
						control.location + ".valueOptions"
					};
				}
				if (IsIdlessLocalToggle(control, referencedControls))
				{
					descriptor.defaultValue = false;
					row.valueRoute = ValueRoute::kLocalUiState;
					++mapped.localUiStateRows;
				}
				else
				{
					row.binding = MapBinding(
						control,
						id,
						descriptor.defaultValue,
						a_diag);
				}
				if (!row.binding && control.valueOptions &&
					control.valueOptions->sourceType &&
					row.valueRoute != ValueRoute::kLocalUiState)
					row.unmappedSource = control.valueOptions->sourceType;
				mapped.rows.push_back(std::move(row));
				if (control.type == ControlType::kHidden)
					continue;
				auto& group = mapped.settings.groups[*currentGroup];
				group.settings.push_back(std::move(descriptor));
				group.rows.emplace_back(
					dmui::SettingGroup::SettingIndex{
						group.settings.size() - 1
					});
				++descriptorCount;

				DiagnoseUnsupported(
					control,
					a_diag);
				if (control.type != ControlType::kText &&
					control.html.value_or(false))
				{
					a_diag.Add(
						DiagnosticSeverity::kWarning,
						control.location + ".html",
						"HTML text presentation is not represented by settings controls");
				}
				if (control.type != ControlType::kText &&
					control.alignment)
				{
					a_diag.Add(
						DiagnosticSeverity::kWarning,
						control.location + ".align",
						"text alignment is not represented by settings controls");
				}
			}

			if (descriptorCount == 0)
			{
				const auto hasUnsupportedImages = std::ranges::any_of(
					a_page.controls,
					[](const Control& a_control) {
						return a_control.type == ControlType::kImage;
					});
				if (hasUnsupportedImages)
					mapped.settings.notes.push_back({
						"This page contains only unrendered SWF components.",
						false
					});
				else
					a_diag.Add(
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
		std::string& a_displayName,
		std::vector<MappedPage>& a_pages,
		std::vector<Diagnostic>& a_diagnostics,
		const TextResolver& a_textResolver)
	{
		detail::Diagnostics diagnostics{ std::string{ a_source }, a_diagnostics };
		DisplayTextResolver textResolver{ a_textResolver, diagnostics };
		a_displayName = a_configuration.displayName.empty() ?
			a_configuration.modName :
			textResolver.Resolve(a_configuration.displayName, "$.displayName");
		a_pages.reserve(a_pages.size() + a_configuration.pages.size());
		for (const auto& page : a_configuration.pages)
		{
			a_pages.push_back(MapPage(
				page, a_displayName, diagnostics, textResolver));
		}
	}
}
