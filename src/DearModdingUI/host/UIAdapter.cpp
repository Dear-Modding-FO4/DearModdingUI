#include "UIAdapterInternal.h"
#include <DearModdingUI/host/Hotkeys.h>
#include <DearModdingUI/host/PointerRouting.h>
#include <DearModdingUI/host/RenderExecution.h>
#include <DearModdingUI/host/ShortcutChord.h>
#include <DearModdingUI/host/UIAdapter.h>
#include <DearModdingUI/UIBindings.generated.h>
#include <DearModdingUI/controls/TextSpans.h>
#include <DearModdingUI/controls/Tooltip.h>
#include <DearModdingUI/presentation/Theme.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace DearModdingUI::UI
{
	namespace
	{
		struct ListClipperEntry
		{
			uint64_t token;
			DMUI_ClientHandle client;
			ImGuiWindow* window;
			ImGuiTable* table;
			// ImGui retains back-pointers, so growing the stack must not move clippers.
			std::unique_ptr<ImGuiListClipper> clipper;
		};

		thread_local std::vector<ListClipperEntry> s_listClippers;
		thread_local uint64_t s_nextListClipperToken{ 1 };
	}

	ListClipperScope::ListClipperScope() noexcept :
		m_depth(s_listClippers.size())
	{}

	ListClipperScope::~ListClipperScope() noexcept
	{
		while (s_listClippers.size() > m_depth)
		{
			auto& clipper = *s_listClippers.back().clipper;
			// The client may have left another window/table current; do not seek there.
			clipper.ItemsCount = -1;
			clipper.End();
			s_listClippers.pop_back();
		}
	}

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

		[[nodiscard]] bool Finite(const DMUI_Placement& a_placement) noexcept
		{
			return std::isfinite(a_placement.position.x) &&
				std::isfinite(a_placement.position.y) &&
				std::isfinite(a_placement.pivot.x) &&
				std::isfinite(a_placement.pivot.y);
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
			a_metrics->fontSizeBase = style.FontSizeBase;
			a_metrics->alpha = style.Alpha;
			a_metrics->frameRounding = style.FrameRounding;
			a_metrics->frameBorderSize = style.FrameBorderSize;
			a_metrics->sectionGap = Theme::Layout().sectionGap;
			a_metrics->panelPadding = Stable(Theme::Layout().panelPadding);
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL GetThemeTokens(
			DMUI_ClientHandle a_client,
			DMUI_ThemeTokens* a_tokens) noexcept
		{
			if (!a_tokens)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			*a_tokens = Theme::TokenSnapshot();
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL GetTime(
			DMUI_ClientHandle a_client,
			double* a_seconds) noexcept
		{
			if (!a_seconds)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_seconds = 0.0;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			*a_seconds = ImGui::GetTime();
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL GetDeltaTime(
			DMUI_ClientHandle a_client,
			float* a_seconds) noexcept
		{
			if (!a_seconds)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_seconds = 0.0f;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			*a_seconds = ImGui::GetIO().DeltaTime;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL GetPointerState(
			DMUI_ClientHandle a_client,
			DMUI_PointerState* a_state) noexcept
		{
			if (!a_state)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_state = {};
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			const auto& io = ImGui::GetIO();
			// Placement works on any page; the edges stay with the owner.
			a_state->viewport = Stable(io.DisplaySize);
			if (ImGui::IsMousePosValid(&io.MousePos))
				a_state->position = Stable(io.MousePos);
			const auto frame = PointerRouting::FrameFor(RenderExecution::ActivePage());
			if (!frame)
				return DMUI_RESULT_OK;

			a_state->active = 1u;
			a_state->captured = io.WantCaptureMouse ? 1u : 0u;
			a_state->wheel = frame->wheel;
			a_state->down = frame->down;
			a_state->pressed = frame->pressed;
			a_state->released = frame->released;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL Shortcut(
			DMUI_ClientHandle a_client,
			const char* a_chord,
			DMUI_UIShortcutFlags a_flags,
			uint32_t* a_pressed) noexcept
		{
			if (!a_chord || !a_pressed)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_pressed = 0u;
			if ((a_flags & ~DMUI_UI_SHORTCUT_FLAGS_REPEAT) != 0)
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			const auto chord = ParseShortcutChord(a_chord);
			if (!chord)
				return DMUI_RESULT_INVALID_ARGUMENT;
			// Focused routing keeps an active text field's own keys.
			*a_pressed = ShortcutPressed(
				*chord,
				(a_flags & DMUI_UI_SHORTCUT_FLAGS_REPEAT) ? ImGuiInputFlags_Repeat : 0) ? 1u : 0u;
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

		DMUI_Result DMUI_CALL EndTable(DMUI_ClientHandle a_client) noexcept
		{
			return EndDrawWindow(a_client, &ImGui::EndTable, true);
		}

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

		DMUI_Result DMUI_CALL BeginItemTooltip(
			DMUI_ClientHandle a_client,
			uint32_t* a_visible) noexcept
		{
			if (!a_visible)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_visible = 0u;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			*a_visible = Tooltip::BeginItem() ? 1u : 0u;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL BeginTooltipAt(
			DMUI_ClientHandle a_client,
			DMUI_Placement a_placement,
			uint32_t* a_visible) noexcept
		{
			if (!a_visible)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_visible = 0u;
			if (!Finite(a_placement))
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			ImGui::SetNextWindowPos(
				Native(a_placement.position), ImGuiCond_Always, Native(a_placement.pivot));
			*a_visible = ImGui::BeginTooltip() ? 1u : 0u;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL EndTooltip(DMUI_ClientHandle a_client) noexcept
		{
			return EndDrawWindow(a_client, &ImGui::EndTooltip);
		}

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
			size_t a_textLength,
			const DMUI_TextSpan* a_spans,
			uint32_t a_spanCount,
			const DMUI_TextOptions* a_options) noexcept
		{
			const auto validation = ValidateText(
				a_client, a_text, a_textLength);
			if (validation != DMUI_RESULT_OK)
				return validation;
			constexpr DMUI_TextOptions defaults{};
			const auto& options = a_options ? *a_options : defaults;
			if ((!a_spans && a_spanCount != 0) || !ValidTextOptions(options))
				return DMUI_RESULT_INVALID_ARGUMENT;
			if (a_spanCount == 0 && options.flags == 0 && options.wrapWidth == 0.0f)
			{
				// Plain text keeps ImGui's clipped, unbounded path.
				ImGui::TextUnformatted(a_text, a_text + a_textLength);
				return DMUI_RESULT_OK;
			}
			const std::string_view text{ a_text, a_textLength };
			const std::span<const DMUI_TextSpan> spans{
				a_spanCount ? a_spans : nullptr, a_spanCount };
			const auto valid = ValidateTextSpans(text, spans, options);
			return valid != DMUI_RESULT_OK ?
				valid : DrawTextSpans(text, spans, options);
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


		DMUI_Result DMUI_CALL ListClipperBegin(
			DMUI_ClientHandle a_client,
			int32_t a_itemsCount,
			float a_itemsHeight,
			uint64_t* a_clipper) noexcept
		{
			if (!a_clipper)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_clipper = 0;
			if (a_itemsCount < 0 ||
				a_itemsCount == (std::numeric_limits<int32_t>::max)() ||
				!std::isfinite(a_itemsHeight))
				return DMUI_RESULT_INVALID_ARGUMENT;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			if (!GImGui || !GImGui->CurrentWindow)
				return DMUI_RESULT_HOST_NOT_READY;
			if (!s_nextListClipperToken)
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			try
			{
				s_listClippers.push_back({
					s_nextListClipperToken,
					a_client,
					GImGui->CurrentWindow,
					GImGui->CurrentTable,
					std::make_unique<ImGuiListClipper>()
				});
			}
			catch (const std::bad_alloc&)
			{
				return DMUI_RESULT_RESOURCE_EXHAUSTED;
			}
			catch (...)
			{
				return DMUI_RESULT_CALLBACK_FAILED;
			}
			auto& entry = s_listClippers.back();
			entry.clipper->Begin(
				a_itemsCount, a_itemsHeight > 0.0f ? a_itemsHeight : -1.0f);
			*a_clipper = s_nextListClipperToken++;
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL ListClipperStep(
			DMUI_ClientHandle a_client,
			uint64_t a_clipper,
			uint32_t* a_stepping,
			int32_t* a_displayStart,
			int32_t* a_displayEnd) noexcept
		{
			if (!a_stepping || !a_displayStart || !a_displayEnd)
				return DMUI_RESULT_INVALID_ARGUMENT;
			*a_stepping = 0;
			*a_displayStart = *a_displayEnd = 0;
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			if (s_listClippers.empty())
				return DMUI_RESULT_UNBALANCED_BRACKET;
			auto& entry = s_listClippers.back();
			if (entry.token != a_clipper || entry.client != a_client ||
				!GImGui || entry.window != GImGui->CurrentWindow ||
				entry.table != GImGui->CurrentTable)
				return DMUI_RESULT_UNBALANCED_BRACKET;
			if (entry.clipper->Step())
			{
				*a_stepping = 1;
				*a_displayStart = entry.clipper->DisplayStart;
				*a_displayEnd = entry.clipper->DisplayEnd;
			}
			else
			{
				s_listClippers.pop_back();
			}
			return DMUI_RESULT_OK;
		}

		DMUI_Result DMUI_CALL ListClipperEnd(
			DMUI_ClientHandle a_client,
			uint64_t a_clipper) noexcept
		{
			const auto validation = Validate(a_client);
			if (validation != DMUI_RESULT_OK)
				return validation;
			const auto entry = std::find_if(
				s_listClippers.begin(), s_listClippers.end(),
				[a_clipper](const ListClipperEntry& a_entry) {
					return a_entry.token == a_clipper;
				});
			if (entry == s_listClippers.end())
				return DMUI_RESULT_OK;
			if (&*entry != &s_listClippers.back() || entry->client != a_client ||
				!GImGui || entry->window != GImGui->CurrentWindow ||
				entry->table != GImGui->CurrentTable)
				return DMUI_RESULT_UNBALANCED_BRACKET;
			entry->clipper->End();
			s_listClippers.pop_back();
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
