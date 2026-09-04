#include <DearModdingUI/MCM/PapyrusValue.h>

#include <RE/B/BSScriptUtil.h>

#include <charconv>
#include <cmath>
#include <limits>
#include <utility>

namespace DearModdingUI::MCM
{
	PapyrusResultCallback::PapyrusResultCallback(
		Function a_function,
		CancelFunction a_cancel) :
		function_(std::move(a_function)),
		cancel_(std::move(a_cancel))
	{}

	void PapyrusResultCallback::CallQueued() {}
	void PapyrusResultCallback::CallCanceled()
	{
		if (cancel_)
			cancel_();
		else
			function_({});
	}
	void PapyrusResultCallback::StartMultiDispatch() {}
	void PapyrusResultCallback::EndMultiDispatch() {}

	void PapyrusResultCallback::operator()(RE::BSScript::Variable a_result)
	{
		function_(std::move(a_result));
	}

	std::optional<RE::BSScript::Variable> ToPapyrus(
		const dmui::SettingValue& a_value,
		SourceValueKind a_kind)
	{
		RE::BSScript::Variable result;
		switch (a_kind)
		{
		case SourceValueKind::kBool:
			if (const auto* value = std::get_if<bool>(&a_value))
				result = *value;
			else
				return std::nullopt;
			break;
		case SourceValueKind::kInt:
		{
			int64_t value{};
			if (const auto* integer = std::get_if<int64_t>(&a_value))
				value = *integer;
			else if (const auto* integer = std::get_if<uint64_t>(&a_value);
				integer &&
				*integer <= static_cast<uint64_t>(
					(std::numeric_limits<int32_t>::max)()))
				value = static_cast<int64_t>(*integer);
			else if (const auto* text = std::get_if<std::string>(&a_value))
			{
				const auto converted = std::from_chars(
					text->data(),
					text->data() + text->size(),
					value);
				if (converted.ec != std::errc{} ||
					converted.ptr != text->data() + text->size())
					return std::nullopt;
			}
			else
				return std::nullopt;
			if (value < (std::numeric_limits<int32_t>::min)() ||
				value > (std::numeric_limits<int32_t>::max)())
				return std::nullopt;
			result = static_cast<int32_t>(value);
			break;
		}
		case SourceValueKind::kFloat:
			if (const auto* value = std::get_if<double>(&a_value);
				value && std::isfinite(*value) &&
				*value >= -(std::numeric_limits<float>::max)() &&
				*value <= (std::numeric_limits<float>::max)())
				result = static_cast<float>(*value);
			else
				return std::nullopt;
			break;
		case SourceValueKind::kString:
			if (const auto* value = std::get_if<std::string>(&a_value))
				result = RE::BSFixedString{ *value };
			else
				return std::nullopt;
			break;
		case SourceValueKind::kNone:
			return std::nullopt;
		}
		return result;
	}

	std::optional<dmui::SettingValue> FromPapyrus(
		const RE::BSScript::Variable& a_value,
		const dmui::SettingValue& a_target)
	{
		if (std::holds_alternative<bool>(a_target))
		{
			if (a_value.is<bool>())
				return dmui::SettingValue{ RE::BSScript::get<bool>(a_value) };
			if (a_value.is<int32_t>())
				return dmui::SettingValue{
					RE::BSScript::get<int32_t>(a_value) != 0
				};
			if (a_value.is<float>())
				return dmui::SettingValue{
					RE::BSScript::get<float>(a_value) != 0.0f
				};
		}
		if (a_value.is<int32_t>())
		{
			const auto value = RE::BSScript::get<int32_t>(a_value);
			if (std::holds_alternative<int64_t>(a_target))
				return dmui::SettingValue{ static_cast<int64_t>(value) };
			if (std::holds_alternative<uint64_t>(a_target) && value >= 0)
				return dmui::SettingValue{ static_cast<uint64_t>(value) };
			if (std::holds_alternative<double>(a_target))
				return dmui::SettingValue{ static_cast<double>(value) };
			if (std::holds_alternative<std::string>(a_target))
				return dmui::SettingValue{ std::to_string(value) };
		}
		if (a_value.is<float>() && std::holds_alternative<double>(a_target))
			return dmui::SettingValue{
				static_cast<double>(RE::BSScript::get<float>(a_value))
			};
		if (a_value.is<RE::BSFixedString>() &&
			std::holds_alternative<std::string>(a_target))
			return dmui::SettingValue{
				std::string{
					RE::BSScript::get<RE::BSFixedString>(a_value).c_str()
				}
			};
		return std::nullopt;
	}
}
