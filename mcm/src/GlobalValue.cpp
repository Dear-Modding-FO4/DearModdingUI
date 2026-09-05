#include <DearModdingUI/MCM/GlobalValue.h>

#include <charconv>
#include <cmath>
#include <limits>
#include <type_traits>

namespace DearModdingUI::MCM
{
	std::optional<GlobalFormReference> ParseGlobalFormReference(
		std::string_view a_sourceForm) noexcept
	{
		try
		{
			const auto separator = a_sourceForm.find('|');
			if (separator == std::string_view::npos ||
				separator == 0 ||
				separator + 1 == a_sourceForm.size() ||
				a_sourceForm.find('|', separator + 1) != std::string_view::npos)
				return std::nullopt;

			uint32_t localId{};
			const auto id = a_sourceForm.substr(separator + 1);
			const auto [end, error] = std::from_chars(
				id.data(),
				id.data() + id.size(),
				localId,
				16);
			if (error != std::errc{} || end != id.data() + id.size())
				return std::nullopt;

			return GlobalFormReference{
				std::string{ a_sourceForm.substr(0, separator) },
				localId
			};
		}
		catch (...)
		{
			return std::nullopt;
		}
	}


	std::optional<dmui::SettingValue> GlobalToSettingValue(
		float a_value,
		const dmui::SettingValue& a_target) noexcept
	{
		const auto integral = [a_value]<class T>() noexcept
			-> std::optional<T> {
			if (!std::isfinite(a_value) ||
				static_cast<long double>(a_value) <
					static_cast<long double>((std::numeric_limits<T>::min)()) ||
				static_cast<long double>(a_value) >
					static_cast<long double>((std::numeric_limits<T>::max)()))
				return std::nullopt;
			return static_cast<T>(a_value);
		};

		return std::visit(
			[&]<class T>(const T&) noexcept
				-> std::optional<dmui::SettingValue> {
				if constexpr (std::is_same_v<T, bool>)
					return dmui::SettingValue{ a_value != 0.0f };
				else if constexpr (std::is_same_v<T, double>)
					return dmui::SettingValue{ static_cast<double>(a_value) };
				else if constexpr (std::is_same_v<T, std::string>)
				{
					const auto index = integral.template operator()<int64_t>();
					return index ?
						std::optional{ dmui::SettingValue{
							std::to_string(*index) } } :
						std::nullopt;
				}
				else
				{
					const auto number = integral.template operator()<T>();
					return number ?
						std::optional{ dmui::SettingValue{ *number } } :
						std::nullopt;
				}
			},
			a_target);
	}

	std::optional<float> SettingValueToGlobal(
		const dmui::SettingValue& a_value) noexcept
	{
		const auto inFloatRange = [](long double a_number) noexcept {
			return std::isfinite(a_number) &&
				a_number >= -static_cast<long double>(
					(std::numeric_limits<float>::max)()) &&
				a_number <= static_cast<long double>(
					(std::numeric_limits<float>::max)());
		};

		return std::visit(
			[&]<class T>(const T& a_held) noexcept -> std::optional<float> {
				if constexpr (std::is_same_v<T, bool>)
					return a_held ? 1.0f : 0.0f;
				else if constexpr (std::is_same_v<T, std::string>)
				{
					float parsed{};
					const auto [end, error] = std::from_chars(
						a_held.data(),
						a_held.data() + a_held.size(),
						parsed);
					return error == std::errc{} &&
							end == a_held.data() + a_held.size() &&
							std::isfinite(parsed) ?
						std::optional{ parsed } :
						std::nullopt;
				}
				else
				{
					return inFloatRange(static_cast<long double>(a_held)) ?
						std::optional{ static_cast<float>(a_held) } :
						std::nullopt;
				}
			},
			a_value);
	}
}