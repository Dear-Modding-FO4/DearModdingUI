#include "UIAdapterInternal.h"
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/host/UIAdapter.h>
#include <DearModdingUI/UIBindings.generated.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <string>

namespace DearModdingUI::UI
{
	namespace AdapterInternal
	{
#if defined(DMUI_UI_TESTING)
		thread_local Testing::ValidationHook s_validationHook{};
#endif

		[[nodiscard]] DMUI_Result Validate(
			DMUI_ClientHandle a_client) noexcept
		{
			if (a_client == DMUI_INVALID_CLIENT_HANDLE)
				return DMUI_RESULT_INVALID_ARGUMENT;
#if defined(DMUI_UI_TESTING)
			if (s_validationHook)
				return s_validationHook(a_client);
#endif
			if (!RenderExecution::IsActiveClient(a_client, true))
				return DMUI_RESULT_WRONG_THREAD;
			if (!ImGui::GetCurrentContext())
				return DMUI_RESULT_HOST_NOT_READY;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] bool ValidText(
			const char* a_text,
			size_t a_length) noexcept
		{
			return a_text &&
				a_length <= static_cast<size_t>(
					(std::numeric_limits<ptrdiff_t>::max)());
		}

		[[nodiscard]] DMUI_Result ValidateText(
			DMUI_ClientHandle a_client,
			const char* a_text,
			size_t a_length) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			return ValidText(a_text, a_length) ?
				DMUI_RESULT_OK :
				DMUI_RESULT_INVALID_ARGUMENT;
		}

		[[nodiscard]] DMUI_Result CopyText(
			const char* a_text,
			size_t a_length,
			std::string& a_copy) noexcept
		{
			if (!ValidText(a_text, a_length))
				return DMUI_RESULT_INVALID_ARGUMENT;
			try
			{
				a_copy.assign(a_text, a_length);
				return DMUI_RESULT_OK;
			}
			catch (const std::bad_alloc&)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
			catch (...)
			{
				return DMUI_RESULT_CALLBACK_FAILED;
			}
		}

		[[nodiscard]] DMUI_Result ValidateBuffer(
			char* a_buffer,
			uint32_t a_capacity) noexcept
		{
			if (!a_buffer || a_capacity == 0)
				return DMUI_RESULT_INVALID_ARGUMENT;
			return std::memchr(a_buffer, '\0', a_capacity) ?
				DMUI_RESULT_OK :
				DMUI_RESULT_INVALID_ARGUMENT;
		}

		[[nodiscard]] DMUI_Result ScalarSize(
			DMUI_UIDataType a_type,
			uint32_t& a_size) noexcept
		{
			switch (a_type)
			{
			case DMUI_UI_DATA_TYPE_S8:
			case DMUI_UI_DATA_TYPE_U8:
				a_size = 1u;
				return DMUI_RESULT_OK;
			case DMUI_UI_DATA_TYPE_S16:
			case DMUI_UI_DATA_TYPE_U16:
				a_size = 2u;
				return DMUI_RESULT_OK;
			case DMUI_UI_DATA_TYPE_S32:
			case DMUI_UI_DATA_TYPE_U32:
			case DMUI_UI_DATA_TYPE_FLOAT:
				a_size = 4u;
				return DMUI_RESULT_OK;
			case DMUI_UI_DATA_TYPE_S64:
			case DMUI_UI_DATA_TYPE_U64:
			case DMUI_UI_DATA_TYPE_DOUBLE:
				a_size = 8u;
				return DMUI_RESULT_OK;
			default:
				return DMUI_RESULT_INVALID_ARGUMENT;
			}
		}

		[[nodiscard]] DMUI_Result ValidateScalar(
			DMUI_UIDataType a_type,
			void* a_data,
			uint32_t a_dataSize,
			const void* a_first,
			uint32_t a_firstSize,
			const void* a_second,
			uint32_t a_secondSize) noexcept
		{
			uint32_t required{};
			const auto typeResult = ScalarSize(a_type, required);
			if (typeResult != DMUI_RESULT_OK)
				return typeResult;
			if (!a_data || a_dataSize != required)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto aligned = [required](const void* a_value) noexcept {
				return !a_value ||
					reinterpret_cast<uintptr_t>(a_value) % required == 0u;
			};
			if (!aligned(a_data) ||
				!aligned(a_first) ||
				!aligned(a_second))
				return DMUI_RESULT_INVALID_ARGUMENT;
			if ((a_first && a_firstSize != required) ||
				(!a_first && a_firstSize != 0) ||
				(a_second && a_secondSize != required) ||
				(!a_second && a_secondSize != 0))
				return DMUI_RESULT_INVALID_ARGUMENT;
			return DMUI_RESULT_OK;
		}

		[[nodiscard]] ImVec2 Native(DMUI_Vec2 a_value) noexcept
		{
			return { a_value.x, a_value.y };
		}

		[[nodiscard]] ImVec4 Native(DMUI_Vec4 a_value) noexcept
		{
			return { a_value.x, a_value.y, a_value.z, a_value.w };
		}

		[[nodiscard]] DMUI_Vec2 Stable(ImVec2 a_value) noexcept
		{
			return { a_value.x, a_value.y };
		}

		[[nodiscard]] DMUI_Vec4 Stable(ImVec4 a_value) noexcept
		{
			return { a_value.x, a_value.y, a_value.z, a_value.w };
		}

		[[nodiscard]] ImVec4 StableRGBA(uint32_t a_rgba) noexcept
		{
			constexpr float scale{ 1.0f / 255.0f };
			return {
				static_cast<float>((a_rgba >> 24u) & 0xFFu) * scale,
				static_cast<float>((a_rgba >> 16u) & 0xFFu) * scale,
				static_cast<float>((a_rgba >> 8u) & 0xFFu) * scale,
				static_cast<float>(a_rgba & 0xFFu) * scale
			};
		}

		[[nodiscard]] DMUI_Result TranslateInputFlags(
			DMUI_UIInputTextFlags a_flags,
			ImGuiInputTextFlags& a_native) noexcept
		{
			if ((a_flags & DMUI_UI_INPUT_TEXT_FLAGS_REJECTED_CALLBACK_MASK) != 0)
				return DMUI_RESULT_INVALID_ARGUMENT;
			return Bindings::TranslateInputTextFlags(a_flags, a_native);
		}
	}

	using namespace AdapterInternal;

	namespace Bindings
	{
		DMUI_Result DMUI_CALL GetStyleMetrics(
			DMUI_ClientHandle a_client,
			DMUI_StyleMetrics* a_metrics) noexcept
		{
			if (!a_metrics)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto requestedSize = a_metrics->structSize;
			if (requestedSize < DMUI_STYLE_METRICS_0_1_SIZE)
				return DMUI_RESULT_STRUCT_TOO_SMALL;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;

			const auto& style = ImGui::GetStyle();
			a_metrics->itemSpacing = Stable(style.ItemSpacing);
			a_metrics->framePadding = Stable(style.FramePadding);
			a_metrics->itemInnerSpacing = Stable(style.ItemInnerSpacing);
			a_metrics->cellPadding = Stable(style.CellPadding);
			a_metrics->windowPadding = Stable(style.WindowPadding);
			a_metrics->indentSpacing = style.IndentSpacing;
			a_metrics->scrollbarSize = style.ScrollbarSize;
			if (requestedSize >= DMUI_STYLE_METRICS_FONT_SIZE_BASE_SIZE)
				a_metrics->fontSizeBase = style.FontSizeBase;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL BeginCombo(
			DMUI_ClientHandle a_client,
			const char* a_label,
			const char* a_previewValue,
			DMUI_UIComboFlags a_flags,
			uint32_t* a_visible) noexcept
		{
			if (!a_label || !a_previewValue || !a_visible)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_visible = 0u;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGuiComboFlags flags{};
			const auto translated = TranslateComboFlags(a_flags, flags);
			if (translated != DMUI_RESULT_OK)
				return translated;
			*a_visible = ImGui::BeginCombo(a_label, a_previewValue, flags) ? 1u : 0u;
			return DMUI_RESULT_OK;
		}

#define DMUI_UI_END(name, native)                                      \
		DMUI_Result DMUI_CALL name(DMUI_ClientHandle a_client) noexcept \
		{                                                               \
			const auto validation = Validate(a_client);                  \
			if (validation != DMUI_RESULT_OK)                            \
				return validation;                                      \
			ImGui::native();                                             \
			return DMUI_RESULT_OK;                                       \
		}

		DMUI_UI_END(EndCombo, EndCombo)

		DMUI_Result DMUI_CALL BeginDisabled(
			DMUI_ClientHandle a_client,
			uint32_t a_disabled) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::BeginDisabled(a_disabled != 0);
			return DMUI_RESULT_OK;
		}

		DMUI_UI_END(EndDisabled, EndDisabled)

		DMUI_Result DMUI_CALL BeginTable(
			DMUI_ClientHandle a_client,
			const char* a_id,
			int32_t a_columns,
			DMUI_UITableFlags a_flags,
			DMUI_Vec2 a_outerSize,
			float a_innerWidth,
			uint32_t* a_visible) noexcept
		{
			if (!a_id || !a_visible || a_columns <= 0)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_visible = 0u;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGuiTableFlags flags{};
			const auto translated = TranslateTableFlags(a_flags, flags);
			if (translated != DMUI_RESULT_OK)
				return translated;
			*a_visible = ImGui::BeginTable(
				a_id, a_columns, flags, Native(a_outerSize), a_innerWidth) ?
				1u :
				0u;
			return DMUI_RESULT_OK;
		}

		DMUI_UI_END(EndTable, EndTable)

		DMUI_Result DMUI_CALL BeginTooltip(
			DMUI_ClientHandle a_client,
			uint32_t* a_visible) noexcept
		{
			if (!a_visible)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_visible = 0u;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			*a_visible = ImGui::BeginTooltip() ? 1u : 0u;
			return DMUI_RESULT_OK;
		}

		DMUI_UI_END(EndTooltip, EndTooltip)

		DMUI_Result DMUI_CALL CalcTextSize(
			DMUI_ClientHandle a_client,
			const char* a_text,
			size_t a_textLength,
			uint32_t a_hideTextAfterDoubleHash,
			float a_wrapWidth,
			DMUI_Vec2* a_size) noexcept
		{
			if (!a_size)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_size = {};
			const auto validation = ValidateText(
				a_client, a_text, a_textLength);
			if (validation != DMUI_RESULT_OK)
				return validation;
			*a_size = Stable(ImGui::CalcTextSize(
				a_text,
				a_text + a_textLength,
				a_hideTextAfterDoubleHash != 0,
				a_wrapWidth));
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL Dummy(
			DMUI_ClientHandle a_client,
			DMUI_Vec2 a_size) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::Dummy(Native(a_size));
			return DMUI_RESULT_OK;
		}

#define DMUI_UI_VEC2_GETTER(name, native)                                \
		DMUI_Result DMUI_CALL name(                                       \
			DMUI_ClientHandle a_client, DMUI_Vec2* a_value) noexcept      \
		{                                                                 \
			if (!a_value)                                                  \
				return DMUI_RESULT_INVALID_ARGUMENT;                       \
			*a_value = {};                                                 \
			const auto validation = Validate(a_client);                    \
			if (validation != DMUI_RESULT_OK)                              \
				return validation;                                        \
			*a_value = Stable(ImGui::native());                            \
			return DMUI_RESULT_OK;                                         \
		}

		DMUI_UI_VEC2_GETTER(GetContentRegionAvail, GetContentRegionAvail)
		DMUI_UI_VEC2_GETTER(GetCursorScreenPos, GetCursorScreenPos)

#define DMUI_UI_FLOAT_GETTER(name, native, parameter)                    \
		DMUI_Result DMUI_CALL name(                                       \
			DMUI_ClientHandle a_client, float* parameter) noexcept        \
		{                                                                 \
			if (!parameter)                                                \
				return DMUI_RESULT_INVALID_ARGUMENT;                       \
			*parameter = 0.0f;                                             \
			const auto validation = Validate(a_client);                    \
			if (validation != DMUI_RESULT_OK)                              \
				return validation;                                        \
			*parameter = ImGui::native();                                  \
			return DMUI_RESULT_OK;                                         \
		}

		DMUI_UI_FLOAT_GETTER(GetFontSize, GetFontSize, a_size)
		DMUI_UI_FLOAT_GETTER(GetFrameHeight, GetFrameHeight, a_height)
		DMUI_UI_FLOAT_GETTER(
			GetTextLineHeightWithSpacing,
			GetTextLineHeightWithSpacing,
			a_height)

		DMUI_Result DMUI_CALL GetStyleColor(
			DMUI_ClientHandle a_client,
			DMUI_UIColor a_color,
			DMUI_Vec4* a_value) noexcept
		{
			if (!a_value)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_value = {};
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGuiCol color{};
			const auto translated = TranslateColor(a_color, color);
			if (translated != DMUI_RESULT_OK)
				return translated;
			*a_value = Stable(ImGui::GetStyleColorVec4(color));
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL Indent(
			DMUI_ClientHandle a_client,
			float a_width) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::Indent(a_width);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL IsItemDeactivatedAfterEdit(
			DMUI_ClientHandle a_client,
			uint32_t* a_deactivated) noexcept
		{
			if (!a_deactivated)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_deactivated = 0u;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			*a_deactivated =
				ImGui::IsItemDeactivatedAfterEdit() ? 1u : 0u;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL IsItemHovered(
			DMUI_ClientHandle a_client,
			DMUI_UIHoveredFlags a_flags,
			uint32_t* a_hovered) noexcept
		{
			if (!a_hovered)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_hovered = 0u;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGuiHoveredFlags flags{};
			const auto translated = TranslateHoveredFlags(a_flags, flags);
			if (translated != DMUI_RESULT_OK)
				return translated;
			*a_hovered = ImGui::IsItemHovered(flags) ? 1u : 0u;
			return DMUI_RESULT_OK;
		}

		DMUI_UI_END(PopID, PopID)

		DMUI_Result DMUI_CALL PopStyleColor(
			DMUI_ClientHandle a_client,
			int32_t a_count) noexcept
		{
			if (a_count < 0)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::PopStyleColor(a_count);
			return DMUI_RESULT_OK;
		}

		DMUI_UI_END(PopTextWrapPos, PopTextWrapPos)

		DMUI_Result DMUI_CALL PushIDString(
			DMUI_ClientHandle a_client,
			const char* a_id) noexcept
		{
			if (!a_id)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::PushID(a_id);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL PushIDRange(
			DMUI_ClientHandle a_client,
			const char* a_id,
			size_t a_length) noexcept
		{
			const auto validation = ValidateText(a_client, a_id, a_length);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::PushID(a_id, a_id + a_length);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL PushIDValue(
			DMUI_ClientHandle a_client,
			uint64_t a_id) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::PushID(reinterpret_cast<const void*>(
				static_cast<uintptr_t>(a_id)));
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL PushStyleColorU32(
			DMUI_ClientHandle a_client,
			DMUI_UIColor a_color,
			uint32_t a_rgba) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGuiCol color{};
			const auto translated = TranslateColor(a_color, color);
			if (translated != DMUI_RESULT_OK)
				return translated;
			ImGui::PushStyleColor(color, StableRGBA(a_rgba));
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL PushStyleColor(
			DMUI_ClientHandle a_client,
			DMUI_UIColor a_color,
			DMUI_Vec4 a_value) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGuiCol color{};
			const auto translated = TranslateColor(a_color, color);
			if (translated != DMUI_RESULT_OK)
				return translated;
			ImGui::PushStyleColor(color, Native(a_value));
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL PushTextWrapPos(
			DMUI_ClientHandle a_client,
			float a_localX) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::PushTextWrapPos(a_localX);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL SameLine(
			DMUI_ClientHandle a_client,
			float a_offsetFromStartX,
			float a_spacing) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::SameLine(a_offsetFromStartX, a_spacing);
			return DMUI_RESULT_OK;
		}

		DMUI_UI_END(Separator, Separator)

		DMUI_Result DMUI_CALL SetClipboardText(
			DMUI_ClientHandle a_client,
			const char* a_text,
			size_t a_textLength) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			std::string text;
			const auto copied = CopyText(a_text, a_textLength, text);
			if (copied != DMUI_RESULT_OK)
				return copied;
			ImGui::SetClipboardText(text.c_str());
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL SetCursorScreenPos(
			DMUI_ClientHandle a_client,
			DMUI_Vec2 a_position) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::SetCursorScreenPos(Native(a_position));
			return DMUI_RESULT_OK;
		}

		DMUI_UI_END(SetItemDefaultFocus, SetItemDefaultFocus)

		DMUI_Result DMUI_CALL SetNextItemWidth(
			DMUI_ClientHandle a_client,
			float a_width) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::SetNextItemWidth(a_width);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL SetTooltipText(
			DMUI_ClientHandle a_client,
			const char* a_text,
			size_t a_textLength) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			std::string text;
			const auto copied = CopyText(a_text, a_textLength, text);
			if (copied != DMUI_RESULT_OK)
				return copied;
			ImGui::SetTooltip("%s", text.c_str());
			return DMUI_RESULT_OK;
		}

		DMUI_UI_END(Spacing, Spacing)
		DMUI_UI_END(TableHeadersRow, TableHeadersRow)

		DMUI_Result DMUI_CALL TableNextColumn(
			DMUI_ClientHandle a_client,
			uint32_t* a_visible) noexcept
		{
			if (!a_visible)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_visible = 0u;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			*a_visible = ImGui::TableNextColumn() ? 1u : 0u;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL TableNextRow(
			DMUI_ClientHandle a_client,
			DMUI_UITableRowFlags a_flags,
			float a_minimumHeight) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGuiTableRowFlags flags{};
			const auto translated = TranslateTableRowFlags(a_flags, flags);
			if (translated != DMUI_RESULT_OK)
				return translated;
			ImGui::TableNextRow(flags, a_minimumHeight);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL TableSetColumnIndex(
			DMUI_ClientHandle a_client,
			int32_t a_column,
			uint32_t* a_visible) noexcept
		{
			if (!a_visible)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_visible = 0u;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			*a_visible = ImGui::TableSetColumnIndex(a_column) ? 1u : 0u;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL TableSetupColumn(
			DMUI_ClientHandle a_client,
			const char* a_label,
			DMUI_UITableColumnFlags a_flags,
			float a_initialWidthOrWeight,
			uint32_t a_userId) noexcept
		{
			if (!a_label)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGuiTableColumnFlags flags{};
			const auto translated = TranslateTableColumnFlags(a_flags, flags);
			if (translated != DMUI_RESULT_OK)
				return translated;
			ImGui::TableSetupColumn(
				a_label, flags, a_initialWidthOrWeight, a_userId);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL TableSetupScrollFreeze(
			DMUI_ClientHandle a_client,
			int32_t a_columns,
			int32_t a_rows) noexcept
		{
			if (a_columns < 0 || a_rows < 0)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::TableSetupScrollFreeze(a_columns, a_rows);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL Text(
			DMUI_ClientHandle a_client,
			const char* a_text,
			size_t a_textLength) noexcept
		{
			const auto validation = ValidateText(
				a_client, a_text, a_textLength);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::TextUnformatted(a_text, a_text + a_textLength);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL TextColored(
			DMUI_ClientHandle a_client,
			DMUI_Vec4 a_color,
			const char* a_text,
			size_t a_textLength) noexcept
		{
			const auto validation = ValidateText(
				a_client, a_text, a_textLength);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::PushStyleColor(ImGuiCol_Text, Native(a_color));
			ImGui::TextUnformatted(a_text, a_text + a_textLength);
			ImGui::PopStyleColor();
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL TextDisabled(
			DMUI_ClientHandle a_client,
			const char* a_text,
			size_t a_textLength) noexcept
		{
			const auto validation = ValidateText(
				a_client, a_text, a_textLength);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::PushStyleColor(
				ImGuiCol_Text,
				ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::TextUnformatted(a_text, a_text + a_textLength);
			ImGui::PopStyleColor();
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL TextWrapped(
			DMUI_ClientHandle a_client,
			const char* a_text,
			size_t a_textLength) noexcept
		{
			const auto validation = ValidateText(
				a_client, a_text, a_textLength);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextUnformatted(a_text, a_text + a_textLength);
			ImGui::PopTextWrapPos();
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL Unindent(
			DMUI_ClientHandle a_client,
			float a_width) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::Unindent(a_width);
			return DMUI_RESULT_OK;
		}

		DMUI_UI_END(NewLine, NewLine)

#undef DMUI_UI_END
#undef DMUI_UI_VEC2_GETTER
#undef DMUI_UI_FLOAT_GETTER
	}

	const DMUI_UIAPI& API() noexcept
	{
		static const auto api = Bindings::MakeAPI();
		return api;
	}

	DMUI_Result Query(
		uint32_t a_requestedUIAbi,
		uint32_t a_minimumRevision,
		uint32_t a_minimumTableSize,
		DMUI_UIAPIInfo* a_info) noexcept
	{
		if (!a_info)
			return DMUI_RESULT_INVALID_ARGUMENT;
		const auto requestedInfoSize = a_info->structSize;
		if (requestedInfoSize < DMUI_UI_API_INFO_PREFIX_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;

		const auto& api = API();
		a_info->abiVersion = api.abiVersion;
		a_info->revision = api.revision;
		a_info->tableSize = api.structSize;
		if (requestedInfoSize >= DMUI_UI_API_INFO_1_SIZE)
			a_info->api = nullptr;

		if (a_requestedUIAbi != api.abiVersion ||
			a_minimumRevision > api.revision ||
			a_minimumTableSize > api.structSize)
			return DMUI_RESULT_UNSUPPORTED_ABI;
		if (requestedInfoSize < DMUI_UI_API_INFO_1_SIZE)
			return DMUI_RESULT_STRUCT_TOO_SMALL;
		a_info->api = &api;
		return DMUI_RESULT_OK;
	}

#if defined(DMUI_UI_TESTING)
	namespace Testing
	{
		ValidationOverride::ValidationOverride(
			ValidationHook a_hook) noexcept :
			m_previous(s_validationHook)
		{
			s_validationHook = a_hook;
		}

		ValidationOverride::~ValidationOverride() noexcept
		{
			s_validationHook = m_previous;
		}
	}
#endif
}
