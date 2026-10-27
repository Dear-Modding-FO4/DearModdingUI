#include <DearModdingUI/controls/Numeric.h>

#include <DearModdingUI/Numeric.h>
#include <DearModdingUI/controls/ControlRenderer.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

namespace DearModdingUI
{
	namespace
	{
		inline constexpr size_t kMaxFormatBytes{ 63 };
		inline constexpr size_t kMaxUnitBytes{ 31 };
		inline constexpr size_t kMaxSpecDigits{ 3 };
		inline constexpr DMUI_NumericFlags kKnownFlags{
			DMUI_NUMERIC_HAS_MINIMUM | DMUI_NUMERIC_HAS_MAXIMUM |
			DMUI_NUMERIC_HAS_STEP | DMUI_NUMERIC_CLAMP
		};

		enum class Storage
		{
			kSigned,
			kUnsigned,
			kFloating
		};

		struct TypeInfo
		{
			Storage storage;
			size_t size;
			ImGuiDataType imgui;
			const char* defaultFormat;
			// The representable range, in the member the storage names.
			DMUI_NumericValue lowest;
			DMUI_NumericValue highest;
		};

		[[nodiscard]] constexpr DMUI_NumericValue Signed(int64_t a_value) noexcept
		{
			DMUI_NumericValue value{};
			value.i = a_value;
			return value;
		}

		[[nodiscard]] constexpr DMUI_NumericValue Unsigned(uint64_t a_value) noexcept
		{
			DMUI_NumericValue value{};
			value.u = a_value;
			return value;
		}

		[[nodiscard]] constexpr DMUI_NumericValue Floating(double a_value) noexcept
		{
			DMUI_NumericValue value{};
			value.f = a_value;
			return value;
		}

		[[nodiscard]] std::optional<TypeInfo> InfoFor(DMUI_NumericType a_type) noexcept
		{
			switch (a_type)
			{
			case DMUI_NUMERIC_TYPE_S8:
				return TypeInfo{ Storage::kSigned, 1, ImGuiDataType_S8, "%d",
					Signed(INT8_MIN), Signed(INT8_MAX) };
			case DMUI_NUMERIC_TYPE_U8:
				return TypeInfo{ Storage::kUnsigned, 1, ImGuiDataType_U8, "%u",
					Unsigned(0), Unsigned(UINT8_MAX) };
			case DMUI_NUMERIC_TYPE_S16:
				return TypeInfo{ Storage::kSigned, 2, ImGuiDataType_S16, "%d",
					Signed(INT16_MIN), Signed(INT16_MAX) };
			case DMUI_NUMERIC_TYPE_U16:
				return TypeInfo{ Storage::kUnsigned, 2, ImGuiDataType_U16, "%u",
					Unsigned(0), Unsigned(UINT16_MAX) };
			case DMUI_NUMERIC_TYPE_S32:
				return TypeInfo{ Storage::kSigned, 4, ImGuiDataType_S32, "%d",
					Signed(INT32_MIN), Signed(INT32_MAX) };
			case DMUI_NUMERIC_TYPE_U32:
				return TypeInfo{ Storage::kUnsigned, 4, ImGuiDataType_U32, "%u",
					Unsigned(0), Unsigned(UINT32_MAX) };
			case DMUI_NUMERIC_TYPE_S64:
				return TypeInfo{ Storage::kSigned, 8, ImGuiDataType_S64, "%lld",
					Signed(INT64_MIN), Signed(INT64_MAX) };
			case DMUI_NUMERIC_TYPE_U64:
				return TypeInfo{ Storage::kUnsigned, 8, ImGuiDataType_U64, "%llu",
					Unsigned(0), Unsigned(UINT64_MAX) };
			case DMUI_NUMERIC_TYPE_FLOAT:
				return TypeInfo{ Storage::kFloating, 4, ImGuiDataType_Float, "%.3f",
					Floating(-FLT_MAX), Floating(FLT_MAX) };
			case DMUI_NUMERIC_TYPE_DOUBLE:
				return TypeInfo{ Storage::kFloating, 8, ImGuiDataType_Double, "%.3f",
					Floating(-DBL_MAX), Floating(DBL_MAX) };
			default:
				return std::nullopt;
			}
		}

		[[nodiscard]] bool InRange(
			const TypeInfo& a_info,
			const DMUI_NumericValue& a_value) noexcept
		{
			switch (a_info.storage)
			{
			case Storage::kSigned:
				return a_value.i >= a_info.lowest.i && a_value.i <= a_info.highest.i;
			case Storage::kUnsigned:
				return a_value.u <= a_info.highest.u;
			case Storage::kFloating:
				return std::isfinite(a_value.f) &&
					a_value.f >= a_info.lowest.f && a_value.f <= a_info.highest.f;
			}
			return false;
		}

		// Calls a_function with the C++ type that backs the numeric type.
		template <class Function>
		decltype(auto) WithScalar(DMUI_NumericType a_type, Function&& a_function)
		{
			switch (a_type)
			{
			case DMUI_NUMERIC_TYPE_S8:
				return a_function(int8_t{});
			case DMUI_NUMERIC_TYPE_U8:
				return a_function(uint8_t{});
			case DMUI_NUMERIC_TYPE_S16:
				return a_function(int16_t{});
			case DMUI_NUMERIC_TYPE_U16:
				return a_function(uint16_t{});
			case DMUI_NUMERIC_TYPE_S32:
				return a_function(int32_t{});
			case DMUI_NUMERIC_TYPE_U32:
				return a_function(uint32_t{});
			case DMUI_NUMERIC_TYPE_S64:
				return a_function(int64_t{});
			case DMUI_NUMERIC_TYPE_U64:
				return a_function(uint64_t{});
			case DMUI_NUMERIC_TYPE_FLOAT:
				return a_function(float{});
			default:
				return a_function(double{});
			}
		}

		// Native storage for any numeric type.
		struct Native
		{
			alignas(8) std::byte bytes[8]{};

			void Store(DMUI_NumericType a_type, const DMUI_NumericValue& a_value) noexcept
			{
				WithScalar(a_type, [&](auto a_tag) noexcept {
					using T = decltype(a_tag);
					const auto scalar = dmui::FromNumericValue<T>(a_value);
					std::memcpy(bytes, &scalar, sizeof(T));
				});
			}

			[[nodiscard]] DMUI_NumericValue Load(DMUI_NumericType a_type) const noexcept
			{
				return WithScalar(a_type, [&](auto a_tag) noexcept {
					using T = decltype(a_tag);
					T scalar{};
					std::memcpy(&scalar, bytes, sizeof(T));
					return dmui::ToNumericValue<T>(scalar);
				});
			}
		};

		[[nodiscard]] bool Equal(
			const TypeInfo& a_info,
			const DMUI_NumericValue& a_left,
			const DMUI_NumericValue& a_right) noexcept
		{
			switch (a_info.storage)
			{
			case Storage::kSigned:
				return a_left.i == a_right.i;
			case Storage::kUnsigned:
				return a_left.u == a_right.u;
			case Storage::kFloating:
				return a_left.f == a_right.f;
			}
			return false;
		}

		[[nodiscard]] bool Greater(
			const TypeInfo& a_info,
			const DMUI_NumericValue& a_left,
			const DMUI_NumericValue& a_right) noexcept
		{
			switch (a_info.storage)
			{
			case Storage::kSigned:
				return a_left.i > a_right.i;
			case Storage::kUnsigned:
				return a_left.u > a_right.u;
			case Storage::kFloating:
				return a_left.f > a_right.f;
			}
			return false;
		}

		template <class Class>
		[[nodiscard]] Class Member(const DMUI_NumericValue& a_value) noexcept
		{
			if constexpr (std::same_as<Class, double>)
				return a_value.f;
			else if constexpr (std::same_as<Class, int64_t>)
				return a_value.i;
			else
				return a_value.u;
		}

		template <class Class>
		[[nodiscard]] DMUI_NumericValue Wrap(Class a_value) noexcept
		{
			if constexpr (std::same_as<Class, double>)
				return Floating(a_value);
			else if constexpr (std::same_as<Class, int64_t>)
				return Signed(a_value);
			else
				return Unsigned(a_value);
		}

		// The one place edits are snapped to the step and held inside the bounds.
		template <class Class>
		[[nodiscard]] DMUI_NumericValue Settle(
			const DMUI_NumericDescriptor& a_numeric,
			const TypeInfo& a_info,
			const DMUI_NumericValue& a_edited) noexcept
		{
			auto value = Member<Class>(a_edited);
			if ((a_numeric.flags & DMUI_NUMERIC_HAS_STEP) != 0)
			{
				value = dmui::QuantizeSettingNumber<Class>(
					value,
					dmui::NumericQuantization<Class>{
						Member<Class>(a_numeric.step),
						Member<Class>(a_numeric.stepOrigin)
					});
			}
			if ((a_numeric.flags & DMUI_NUMERIC_CLAMP) != 0)
			{
				dmui::NumericSettingRange<Class> range;
				if ((a_numeric.flags & DMUI_NUMERIC_HAS_MINIMUM) != 0)
					range.minimum = Member<Class>(a_numeric.minimum);
				if ((a_numeric.flags & DMUI_NUMERIC_HAS_MAXIMUM) != 0)
					range.maximum = Member<Class>(a_numeric.maximum);
				value = dmui::ClampSettingNumber<Class>(value, Class{}, range);
			}
			// Quantizing may step past what the type holds.
			return Wrap<Class>(
				(std::clamp)(value, Member<Class>(a_info.lowest), Member<Class>(a_info.highest)));
		}

		[[nodiscard]] DMUI_NumericValue Settle(
			const DMUI_NumericDescriptor& a_numeric,
			const TypeInfo& a_info,
			const DMUI_NumericValue& a_edited) noexcept
		{
			switch (a_info.storage)
			{
			case Storage::kSigned:
				return Settle<int64_t>(a_numeric, a_info, a_edited);
			case Storage::kUnsigned:
				return Settle<uint64_t>(a_numeric, a_info, a_edited);
			case Storage::kFloating:
				return Settle<double>(a_numeric, a_info, a_edited);
			}
			return a_edited;
		}

		[[nodiscard]] bool ConversionFits(Storage a_storage, char a_conversion) noexcept
		{
			constexpr std::string_view floating{ "fFeEgG" };
			constexpr std::string_view integer{ "diuxX" };
			return (a_storage == Storage::kFloating ? floating : integer)
				.find(a_conversion) != std::string_view::npos;
		}

		[[nodiscard]] bool ConsumeDigits(
			std::string_view a_format,
			size_t& a_index,
			std::string& a_spec) noexcept
		{
			size_t digits{};
			while (a_index < a_format.size() &&
				a_format[a_index] >= '0' && a_format[a_index] <= '9')
			{
				if (++digits > kMaxSpecDigits)
					return false;
				a_spec += a_format[a_index++];
			}
			return true;
		}
	}

	std::optional<std::string> BuildNumericFormat(
		const char* a_format,
		const char* a_unit,
		DMUI_NumericType a_type) noexcept
	{
		try
		{
			const auto info = InfoFor(a_type);
			if (!info)
				return std::nullopt;
			const std::string_view format = a_format && a_format[0] != '\0' ?
				std::string_view{ a_format } :
				std::string_view{ info->defaultFormat };
			const std::string_view unit = a_unit ? a_unit : "";
			if (format.size() > kMaxFormatBytes || unit.size() > kMaxUnitBytes)
				return std::nullopt;

			std::string rebuilt;
			size_t conversions{};
			for (size_t index = 0; index < format.size();)
			{
				const auto character = format[index++];
				if (character != '%')
				{
					rebuilt += character;
					continue;
				}
				if (index >= format.size())
					return std::nullopt;
				if (format[index] == '%')
				{
					rebuilt += "%%";
					++index;
					continue;
				}

				std::string spec;
				while (index < format.size() &&
					std::string_view{ "-+ #0" }.find(format[index]) !=
						std::string_view::npos)
					spec += format[index++];
				if (!ConsumeDigits(format, index, spec))
					return std::nullopt;
				if (index < format.size() && format[index] == '.')
				{
					spec += format[index++];
					if (!ConsumeDigits(format, index, spec))
						return std::nullopt;
				}
				// The caller's length modifiers are replaced by the type's own.
				size_t lengths{};
				while (index < format.size() &&
					std::string_view{ "hlLzjt" }.find(format[index]) !=
						std::string_view::npos)
				{
					if (++lengths > 2)
						return std::nullopt;
					++index;
				}
				if (index >= format.size() ||
					!ConversionFits(info->storage, format[index]) ||
					++conversions > 1)
					return std::nullopt;
				rebuilt += '%';
				rebuilt += spec;
				if (info->storage != Storage::kFloating && info->size == 8)
					rebuilt += "ll";
				rebuilt += format[index++];
			}
			if (conversions != 1)
				return std::nullopt;
			for (const auto character : unit)
			{
				rebuilt += character;
				if (character == '%')
					rebuilt += '%';
			}
			return rebuilt;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	DMUI_Result ValidateNumeric(const DMUI_NumericDescriptor* a_numeric) noexcept
	{
		if (!a_numeric || !a_numeric->id || a_numeric->id[0] == '\0' ||
			a_numeric->reserved != 0 ||
			(a_numeric->flags & ~kKnownFlags) != 0 ||
			a_numeric->presentation > DMUI_NUMERIC_PRESENTATION_INPUT ||
			!std::isfinite(a_numeric->width) ||
			!std::isfinite(a_numeric->dragSpeed) || a_numeric->dragSpeed < 0.0f)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto info = InfoFor(a_numeric->type);
		if (!info || !InRange(*info, a_numeric->value))
			return DMUI_RESULT_INVALID_ARGUMENT;

		const auto flags = a_numeric->flags;
		const auto bounded = [&](DMUI_NumericFlags a_flag, const DMUI_NumericValue& a_value) noexcept {
			return (flags & a_flag) == 0 || InRange(*info, a_value);
		};
		if (!bounded(DMUI_NUMERIC_HAS_MINIMUM, a_numeric->minimum) ||
			!bounded(DMUI_NUMERIC_HAS_MAXIMUM, a_numeric->maximum))
			return DMUI_RESULT_INVALID_ARGUMENT;
		if (a_numeric->presentation == DMUI_NUMERIC_PRESENTATION_SLIDER &&
			(flags & (DMUI_NUMERIC_HAS_MINIMUM | DMUI_NUMERIC_HAS_MAXIMUM)) !=
				(DMUI_NUMERIC_HAS_MINIMUM | DMUI_NUMERIC_HAS_MAXIMUM))
			return DMUI_RESULT_INVALID_ARGUMENT;
		if ((flags & DMUI_NUMERIC_HAS_STEP) != 0)
		{
			const auto& step = a_numeric->step;
			const auto positive = info->storage == Storage::kSigned ? step.i > 0 :
				info->storage == Storage::kUnsigned ? step.u > 0 : step.f > 0.0;
			if (!positive || !InRange(*info, step) || !InRange(*info, a_numeric->stepOrigin))
				return DMUI_RESULT_INVALID_ARGUMENT;
		}
		return BuildNumericFormat(a_numeric->format, a_numeric->unit, a_numeric->type) ?
			DMUI_RESULT_OK :
			DMUI_RESULT_INVALID_ARGUMENT;
	}

	DMUI_NumericResult DrawNumeric(const DMUI_NumericDescriptor& a_numeric) noexcept
	{
		DMUI_NumericResult result{ a_numeric.value, 0u, 0u };
		const auto info = InfoFor(a_numeric.type);
		const auto format = BuildNumericFormat(
			a_numeric.format, a_numeric.unit, a_numeric.type);
		if (!info || !format)
			return result;

		Native value;
		value.Store(a_numeric.type, a_numeric.value);
		const auto before = value.Load(a_numeric.type);

		// A reversed pair is swapped; the native widgets expect ascending bounds.
		auto lower = a_numeric.minimum;
		auto upper = a_numeric.maximum;
		const auto hasLower = (a_numeric.flags & DMUI_NUMERIC_HAS_MINIMUM) != 0;
		const auto hasUpper = (a_numeric.flags & DMUI_NUMERIC_HAS_MAXIMUM) != 0;
		if (hasLower && hasUpper && Greater(*info, lower, upper))
			std::swap(lower, upper);
		Native minimum;
		Native maximum;
		minimum.Store(a_numeric.type, lower);
		maximum.Store(a_numeric.type, upper);
		const auto typed = a_numeric.presentation != DMUI_NUMERIC_PRESENTATION_INPUT;

		NumericContent content;
		content.presentation = a_numeric.presentation;
		content.dataType = info->imgui;
		content.value = value.bytes;
		content.minimum = typed && hasLower ? minimum.bytes : nullptr;
		content.maximum = typed && hasUpper ? maximum.bytes : nullptr;
		content.format = format->c_str();
		content.speed = a_numeric.dragSpeed > 0.0f ? a_numeric.dragSpeed :
			(info->storage == Storage::kFloating ? 0.01f : 1.0f);
		content.clamp = (a_numeric.flags & DMUI_NUMERIC_CLAMP) != 0;

		ImGui::PushID(a_numeric.id);
		if (a_numeric.width != 0.0f)
			ImGui::SetNextItemWidth(a_numeric.width);
		const auto edited = CurrentControlRenderer().DrawNumeric(content);
		const auto completed = ImGui::IsItemDeactivatedAfterEdit();
		ImGui::PopID();

		if (edited)
		{
			// Round-trips through the native width so float compares as stored.
			Native settled;
			settled.Store(a_numeric.type, Settle(a_numeric, *info, value.Load(a_numeric.type)));
			const auto after = settled.Load(a_numeric.type);
			if (!Equal(*info, after, before))
			{
				result.value = after;
				result.changed = 1u;
			}
		}
		result.completed = completed ? 1u : 0u;
		return result;
	}
}
