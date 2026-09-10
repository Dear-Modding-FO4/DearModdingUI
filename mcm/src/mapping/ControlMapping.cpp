#include "ControlMapping.h"

#include "../support/SliderNormalization.h"

#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

namespace DearModdingUI::MCM::detail
{
	namespace
	{
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
	}

	bool UsesSignedNumbers(const Control& a_control)
	{
		return a_control.valueOptions &&
			a_control.valueOptions->sourceType &&
			a_control.valueOptions->sourceType->value ==
				SourceValueKind::kInt;
	}

	bool HasValidIntegerSliderParameters(
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
		Diagnostics& a_diagnostics)
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
				a_diagnostics.Add(
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
		Diagnostics& a_diagnostics,
		MappedRow* a_sliderRow)
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
				if (a_sliderRow && options.minimum && options.maximum)
				{
					a_sliderRow->sliderNormalization =
						DoubleSliderNormalization{
							*options.minimum,
							*options.maximum,
							*options.step
						};
				}
				else
				{
					mapped.quantization = dmui::NumericQuantization<double>{
						*options.step,
						a_control.type == ControlType::kSlider ?
							0.0 :
							options.minimum.value_or(0.0)
					};
				}
			}
			else if (options.step)
			{
				a_diagnostics.Add(
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
					a_diagnostics.Add(
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
		if (a_sliderRow && a_sliderRow->sliderNormalization)
			a_descriptor.defaultValue = NormalizeSliderValue(
				*a_sliderRow->sliderNormalization,
				a_descriptor.defaultValue);
		a_descriptor.control = std::move(mapped);
	}

	void MapSignedControl(
		const Control& a_control,
		dmui::SettingDescriptor& a_descriptor,
		Diagnostics& a_diagnostics,
		MappedRow* a_sliderRow)
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
				a_diagnostics.Add(
					DiagnosticSeverity::kWarning,
					a_control.location + ".valueOptions.min",
					"integer setting has a non-integral or out-of-range minimum");
			}
			if (options.maximum && !maximum)
			{
				a_diagnostics.Add(
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
				if (a_sliderRow && minimum && maximum)
				{
					a_sliderRow->sliderNormalization =
						SignedSliderNormalization{
							*minimum,
							*maximum,
							*step
						};
				}
				else
				{
					mapped.quantization = dmui::NumericQuantization<int64_t>{
						*step,
						a_control.type == ControlType::kSlider ?
							0 :
							minimum.value_or(0)
					};
				}
			}
			else if (options.step)
			{
				a_diagnostics.Add(
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
					a_diagnostics.Add(
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
		if (a_sliderRow && a_sliderRow->sliderNormalization)
			a_descriptor.defaultValue = NormalizeSliderValue(
				*a_sliderRow->sliderNormalization,
				a_descriptor.defaultValue);
		a_descriptor.control = std::move(mapped);
	}
}
