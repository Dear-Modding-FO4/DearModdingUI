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

	SourceValueKind ResolveSourceValueKind(
		SourceValueKind a_declared,
		const dmui::SettingValue& a_default) noexcept
	{
		return std::visit(
			[a_declared]<class T>(const T&) noexcept {
				if constexpr (std::is_same_v<T, bool>)
					return SourceValueKind::kBool;
				else if constexpr (std::is_same_v<T, double>)
					return SourceValueKind::kFloat;
				else if constexpr (std::is_same_v<T, int64_t> ||
								   std::is_same_v<T, uint64_t>)
					return SourceValueKind::kInt;
				else if constexpr (std::is_same_v<T, std::string>)
					return SourceValueKind::kString;
				else
					return a_declared;
			},
			a_default);
	}

	std::optional<dmui::SettingValue> GlobalToSettingValue(
		float a_value,
		SourceValueKind a_kind) noexcept
	{
		switch (a_kind)
		{
		case SourceValueKind::kBool:
			return dmui::SettingValue{ a_value != 0.0f };
		case SourceValueKind::kInt:
			if (!std::isfinite(a_value) ||
				static_cast<long double>(a_value) <
					static_cast<long double>((std::numeric_limits<int64_t>::min)()) ||
				static_cast<long double>(a_value) >
					static_cast<long double>((std::numeric_limits<int64_t>::max)()))
				return std::nullopt;
			return dmui::SettingValue{ static_cast<int64_t>(a_value) };
		case SourceValueKind::kFloat:
			return dmui::SettingValue{ static_cast<double>(a_value) };
		case SourceValueKind::kString:
			if (!std::isfinite(a_value) ||
				static_cast<long double>(a_value) <
					static_cast<long double>((std::numeric_limits<int64_t>::min)()) ||
				static_cast<long double>(a_value) >
					static_cast<long double>((std::numeric_limits<int64_t>::max)()))
				return std::nullopt;
			return dmui::SettingValue{
				std::to_string(static_cast<int64_t>(a_value))
			};
		default:
			return std::nullopt;
		}
	}

	std::optional<float> SettingValueToGlobal(
		const dmui::SettingValue& a_value,
		SourceValueKind a_kind) noexcept
	{
		const auto inFloatRange = [](long double a_number) noexcept {
			return std::isfinite(a_number) &&
				a_number >= -static_cast<long double>(
					(std::numeric_limits<float>::max)()) &&
				a_number <= static_cast<long double>(
					(std::numeric_limits<float>::max)());
		};

		switch (a_kind)
		{
		case SourceValueKind::kBool:
			if (const auto value = std::get_if<bool>(&a_value))
				return *value ? 1.0f : 0.0f;
			break;
		case SourceValueKind::kInt:
			if (const auto value = std::get_if<int64_t>(&a_value);
				value && inFloatRange(static_cast<long double>(*value)))
				return static_cast<float>(*value);
			if (const auto value = std::get_if<uint64_t>(&a_value);
				value && inFloatRange(static_cast<long double>(*value)))
				return static_cast<float>(*value);
			break;
		case SourceValueKind::kFloat:
			if (const auto value = std::get_if<double>(&a_value);
				value && inFloatRange(static_cast<long double>(*value)))
				return static_cast<float>(*value);
			break;
		case SourceValueKind::kString:
			if (const auto text = std::get_if<std::string>(&a_value))
			{
				float value{};
				const auto [end, error] = std::from_chars(
					text->data(),
					text->data() + text->size(),
					value);
				if (error == std::errc{} &&
					end == text->data() + text->size() &&
					std::isfinite(value))
					return value;
			}
			break;
		default:
			break;
		}
		return std::nullopt;
	}
}
