#pragma once

#include <DearModdingUI/MCM/Compatibility.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace DearModdingUI::MCM::detail
{
	[[nodiscard]] inline int64_t SnapSignedSliderValue(
		int64_t a_value,
		int64_t a_step) noexcept
	{
		const auto quotient = a_value / a_step;
		const auto remainder = a_value % a_step;
		const auto base = quotient * a_step;
		if (remainder >= 0)
		{
			const auto roundUpThreshold = a_step / 2 + a_step % 2;
			if (remainder < roundUpThreshold)
				return base;
			const auto maximumQuotient =
				(std::numeric_limits<int64_t>::max)() / a_step;
			return quotient >= maximumQuotient ?
				base :
				(quotient + 1) * a_step;
		}

		const auto magnitude = -remainder;
		if (magnitude <= a_step / 2)
			return base;
		const auto minimumQuotient =
			(std::numeric_limits<int64_t>::min)() / a_step;
		return quotient <= minimumQuotient ?
			base :
			(quotient - 1) * a_step;
	}

	[[nodiscard]] inline dmui::SettingValue NormalizeSliderValue(
		const SliderNormalization& a_normalization,
		dmui::SettingValue a_value)
	{
		return std::visit(
			[&](const auto& a_parameters) -> dmui::SettingValue {
				if (a_parameters.maximum < a_parameters.minimum ||
					a_parameters.step <= 0)
					throw std::invalid_argument("invalid MCM slider normalization");
				using T = std::remove_cvref_t<decltype(a_parameters)>;
				if constexpr (std::same_as<T, DoubleSliderNormalization>)
				{
					const auto value = (std::clamp)(
						std::get<double>(a_value),
						a_parameters.minimum,
						a_parameters.maximum);
					const auto snapped = std::floor(
						value / a_parameters.step + 0.5) *
						a_parameters.step;
					if (!std::isfinite(snapped))
						throw std::invalid_argument("MCM slider value cannot be snapped");
					return snapped;
				}
				else
				{
					const auto value = (std::clamp)(
						std::get<int64_t>(a_value),
						a_parameters.minimum,
						a_parameters.maximum);
					return SnapSignedSliderValue(value, a_parameters.step);
				}
			},
			a_normalization);
	}
}
