#pragma once

// Generated from schema/ui-contract.json; do not edit.

#include <DearModdingUI/CUIAPI.h>

#include <imgui/imgui.h>

namespace DearModdingUI::UI::Bindings
{
	[[nodiscard]] inline DMUI_Result TranslateColor(
		DMUI_UIColor a_value,
		ImGuiCol& a_native) noexcept
	{
		switch (a_value)
		{
		case DMUI_UI_COLOR_TEXT:
			a_native = ImGuiCol_Text;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TEXT_DISABLED:
			a_native = ImGuiCol_TextDisabled;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_WINDOW_BG:
			a_native = ImGuiCol_WindowBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_CHILD_BG:
			a_native = ImGuiCol_ChildBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_POPUP_BG:
			a_native = ImGuiCol_PopupBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_BORDER:
			a_native = ImGuiCol_Border;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_BORDER_SHADOW:
			a_native = ImGuiCol_BorderShadow;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_FRAME_BG:
			a_native = ImGuiCol_FrameBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_FRAME_BG_HOVERED:
			a_native = ImGuiCol_FrameBgHovered;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_FRAME_BG_ACTIVE:
			a_native = ImGuiCol_FrameBgActive;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TITLE_BG:
			a_native = ImGuiCol_TitleBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TITLE_BG_ACTIVE:
			a_native = ImGuiCol_TitleBgActive;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TITLE_BG_COLLAPSED:
			a_native = ImGuiCol_TitleBgCollapsed;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_MENU_BAR_BG:
			a_native = ImGuiCol_MenuBarBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_SCROLLBAR_BG:
			a_native = ImGuiCol_ScrollbarBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_SCROLLBAR_GRAB:
			a_native = ImGuiCol_ScrollbarGrab;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_SCROLLBAR_GRAB_HOVERED:
			a_native = ImGuiCol_ScrollbarGrabHovered;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_SCROLLBAR_GRAB_ACTIVE:
			a_native = ImGuiCol_ScrollbarGrabActive;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_CHECK_MARK:
			a_native = ImGuiCol_CheckMark;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_CHECKBOX_SELECTED_BG:
			a_native = ImGuiCol_CheckboxSelectedBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_SLIDER_GRAB:
			a_native = ImGuiCol_SliderGrab;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_SLIDER_GRAB_ACTIVE:
			a_native = ImGuiCol_SliderGrabActive;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_BUTTON:
			a_native = ImGuiCol_Button;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_BUTTON_HOVERED:
			a_native = ImGuiCol_ButtonHovered;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_BUTTON_ACTIVE:
			a_native = ImGuiCol_ButtonActive;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_HEADER:
			a_native = ImGuiCol_Header;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_HEADER_HOVERED:
			a_native = ImGuiCol_HeaderHovered;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_HEADER_ACTIVE:
			a_native = ImGuiCol_HeaderActive;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_SEPARATOR:
			a_native = ImGuiCol_Separator;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_SEPARATOR_HOVERED:
			a_native = ImGuiCol_SeparatorHovered;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_SEPARATOR_ACTIVE:
			a_native = ImGuiCol_SeparatorActive;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_RESIZE_GRIP:
			a_native = ImGuiCol_ResizeGrip;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_RESIZE_GRIP_HOVERED:
			a_native = ImGuiCol_ResizeGripHovered;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_RESIZE_GRIP_ACTIVE:
			a_native = ImGuiCol_ResizeGripActive;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_INPUT_TEXT_CURSOR:
			a_native = ImGuiCol_InputTextCursor;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TAB_HOVERED:
			a_native = ImGuiCol_TabHovered;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TAB:
			a_native = ImGuiCol_Tab;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TAB_SELECTED:
			a_native = ImGuiCol_TabSelected;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TAB_SELECTED_OVERLINE:
			a_native = ImGuiCol_TabSelectedOverline;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TAB_DIMMED:
			a_native = ImGuiCol_TabDimmed;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TAB_DIMMED_SELECTED:
			a_native = ImGuiCol_TabDimmedSelected;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TAB_DIMMED_SELECTED_OVERLINE:
			a_native = ImGuiCol_TabDimmedSelectedOverline;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_DOCKING_PREVIEW:
			a_native = ImGuiCol_DockingPreview;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_DOCKING_EMPTY_BG:
			a_native = ImGuiCol_DockingEmptyBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_PLOT_LINES:
			a_native = ImGuiCol_PlotLines;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_PLOT_LINES_HOVERED:
			a_native = ImGuiCol_PlotLinesHovered;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_PLOT_HISTOGRAM:
			a_native = ImGuiCol_PlotHistogram;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_PLOT_HISTOGRAM_HOVERED:
			a_native = ImGuiCol_PlotHistogramHovered;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TABLE_HEADER_BG:
			a_native = ImGuiCol_TableHeaderBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TABLE_BORDER_STRONG:
			a_native = ImGuiCol_TableBorderStrong;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TABLE_BORDER_LIGHT:
			a_native = ImGuiCol_TableBorderLight;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TABLE_ROW_BG:
			a_native = ImGuiCol_TableRowBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TABLE_ROW_BG_ALT:
			a_native = ImGuiCol_TableRowBgAlt;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TEXT_LINK:
			a_native = ImGuiCol_TextLink;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TEXT_SELECTED_BG:
			a_native = ImGuiCol_TextSelectedBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_TREE_LINES:
			a_native = ImGuiCol_TreeLines;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_DRAG_DROP_TARGET:
			a_native = ImGuiCol_DragDropTarget;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_DRAG_DROP_TARGET_BG:
			a_native = ImGuiCol_DragDropTargetBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_UNSAVED_MARKER:
			a_native = ImGuiCol_UnsavedMarker;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_NAV_CURSOR:
			a_native = ImGuiCol_NavCursor;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_NAV_WINDOWING_HIGHLIGHT:
			a_native = ImGuiCol_NavWindowingHighlight;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_NAV_WINDOWING_DIM_BG:
			a_native = ImGuiCol_NavWindowingDimBg;
			return DMUI_RESULT_OK;
		case DMUI_UI_COLOR_MODAL_WINDOW_DIM_BG:
			a_native = ImGuiCol_ModalWindowDimBg;
			return DMUI_RESULT_OK;
		default:
			return DMUI_RESULT_INVALID_ARGUMENT;
		}
	}

	[[nodiscard]] inline DMUI_Result TranslateDataType(
		DMUI_UIDataType a_value,
		ImGuiDataType& a_native) noexcept
	{
		switch (a_value)
		{
		case DMUI_UI_DATA_TYPE_S8:
			a_native = ImGuiDataType_S8;
			return DMUI_RESULT_OK;
		case DMUI_UI_DATA_TYPE_U8:
			a_native = ImGuiDataType_U8;
			return DMUI_RESULT_OK;
		case DMUI_UI_DATA_TYPE_S16:
			a_native = ImGuiDataType_S16;
			return DMUI_RESULT_OK;
		case DMUI_UI_DATA_TYPE_U16:
			a_native = ImGuiDataType_U16;
			return DMUI_RESULT_OK;
		case DMUI_UI_DATA_TYPE_S32:
			a_native = ImGuiDataType_S32;
			return DMUI_RESULT_OK;
		case DMUI_UI_DATA_TYPE_U32:
			a_native = ImGuiDataType_U32;
			return DMUI_RESULT_OK;
		case DMUI_UI_DATA_TYPE_S64:
			a_native = ImGuiDataType_S64;
			return DMUI_RESULT_OK;
		case DMUI_UI_DATA_TYPE_U64:
			a_native = ImGuiDataType_U64;
			return DMUI_RESULT_OK;
		case DMUI_UI_DATA_TYPE_FLOAT:
			a_native = ImGuiDataType_Float;
			return DMUI_RESULT_OK;
		case DMUI_UI_DATA_TYPE_DOUBLE:
			a_native = ImGuiDataType_Double;
			return DMUI_RESULT_OK;
		default:
			return DMUI_RESULT_INVALID_ARGUMENT;
		}
	}

	[[nodiscard]] inline DMUI_Result TranslateComboFlags(
		DMUI_UIComboFlags a_value,
		ImGuiComboFlags& a_native) noexcept
	{
		constexpr uint32_t known{ DMUI_UI_COMBO_FLAGS_POPUP_ALIGN_LEFT | DMUI_UI_COMBO_FLAGS_HEIGHT_SMALL | DMUI_UI_COMBO_FLAGS_HEIGHT_REGULAR | DMUI_UI_COMBO_FLAGS_HEIGHT_LARGE | DMUI_UI_COMBO_FLAGS_HEIGHT_LARGEST | DMUI_UI_COMBO_FLAGS_NO_ARROW_BUTTON | DMUI_UI_COMBO_FLAGS_NO_PREVIEW | DMUI_UI_COMBO_FLAGS_WIDTH_FIT_PREVIEW };
		if ((a_value & ~known) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		constexpr uint32_t heightSmallMask{ DMUI_UI_COMBO_FLAGS_HEIGHT_SMALL | DMUI_UI_COMBO_FLAGS_HEIGHT_REGULAR | DMUI_UI_COMBO_FLAGS_HEIGHT_LARGE | DMUI_UI_COMBO_FLAGS_HEIGHT_LARGEST };
		const auto heightSmallBits = a_value & heightSmallMask;
		if (heightSmallBits != 0 && (heightSmallBits & (heightSmallBits - 1u)) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		a_native = 0;
		if ((a_value & DMUI_UI_COMBO_FLAGS_POPUP_ALIGN_LEFT) != 0)
			a_native |= ImGuiComboFlags_PopupAlignLeft;
		if ((a_value & DMUI_UI_COMBO_FLAGS_HEIGHT_SMALL) != 0)
			a_native |= ImGuiComboFlags_HeightSmall;
		if ((a_value & DMUI_UI_COMBO_FLAGS_HEIGHT_REGULAR) != 0)
			a_native |= ImGuiComboFlags_HeightRegular;
		if ((a_value & DMUI_UI_COMBO_FLAGS_HEIGHT_LARGE) != 0)
			a_native |= ImGuiComboFlags_HeightLarge;
		if ((a_value & DMUI_UI_COMBO_FLAGS_HEIGHT_LARGEST) != 0)
			a_native |= ImGuiComboFlags_HeightLargest;
		if ((a_value & DMUI_UI_COMBO_FLAGS_NO_ARROW_BUTTON) != 0)
			a_native |= ImGuiComboFlags_NoArrowButton;
		if ((a_value & DMUI_UI_COMBO_FLAGS_NO_PREVIEW) != 0)
			a_native |= ImGuiComboFlags_NoPreview;
		if ((a_value & DMUI_UI_COMBO_FLAGS_WIDTH_FIT_PREVIEW) != 0)
			a_native |= ImGuiComboFlags_WidthFitPreview;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] inline DMUI_Result TranslateHoveredFlags(
		DMUI_UIHoveredFlags a_value,
		ImGuiHoveredFlags& a_native) noexcept
	{
		constexpr uint32_t known{ DMUI_UI_HOVERED_FLAGS_CHILD_WINDOWS | DMUI_UI_HOVERED_FLAGS_ROOT_WINDOW | DMUI_UI_HOVERED_FLAGS_ANY_WINDOW | DMUI_UI_HOVERED_FLAGS_NO_POPUP_HIERARCHY | DMUI_UI_HOVERED_FLAGS_DOCK_HIERARCHY | DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_BLOCKED_BY_POPUP | DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_BLOCKED_BY_ACTIVE_ITEM | DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_OVERLAPPED_BY_ITEM | DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_OVERLAPPED_BY_WINDOW | DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_DISABLED | DMUI_UI_HOVERED_FLAGS_NO_NAV_OVERRIDE | DMUI_UI_HOVERED_FLAGS_FOR_TOOLTIP | DMUI_UI_HOVERED_FLAGS_STATIONARY | DMUI_UI_HOVERED_FLAGS_DELAY_NONE | DMUI_UI_HOVERED_FLAGS_DELAY_SHORT | DMUI_UI_HOVERED_FLAGS_DELAY_NORMAL | DMUI_UI_HOVERED_FLAGS_NO_SHARED_DELAY };
		if ((a_value & ~known) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		constexpr uint32_t delayNoneMask{ DMUI_UI_HOVERED_FLAGS_DELAY_NONE | DMUI_UI_HOVERED_FLAGS_DELAY_SHORT | DMUI_UI_HOVERED_FLAGS_DELAY_NORMAL };
		const auto delayNoneBits = a_value & delayNoneMask;
		if (delayNoneBits != 0 && (delayNoneBits & (delayNoneBits - 1u)) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		a_native = 0;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_CHILD_WINDOWS) != 0)
			a_native |= ImGuiHoveredFlags_ChildWindows;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_ROOT_WINDOW) != 0)
			a_native |= ImGuiHoveredFlags_RootWindow;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_ANY_WINDOW) != 0)
			a_native |= ImGuiHoveredFlags_AnyWindow;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_NO_POPUP_HIERARCHY) != 0)
			a_native |= ImGuiHoveredFlags_NoPopupHierarchy;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_DOCK_HIERARCHY) != 0)
			a_native |= ImGuiHoveredFlags_DockHierarchy;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_BLOCKED_BY_POPUP) != 0)
			a_native |= ImGuiHoveredFlags_AllowWhenBlockedByPopup;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_BLOCKED_BY_ACTIVE_ITEM) != 0)
			a_native |= ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_OVERLAPPED_BY_ITEM) != 0)
			a_native |= ImGuiHoveredFlags_AllowWhenOverlappedByItem;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_OVERLAPPED_BY_WINDOW) != 0)
			a_native |= ImGuiHoveredFlags_AllowWhenOverlappedByWindow;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_ALLOW_WHEN_DISABLED) != 0)
			a_native |= ImGuiHoveredFlags_AllowWhenDisabled;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_NO_NAV_OVERRIDE) != 0)
			a_native |= ImGuiHoveredFlags_NoNavOverride;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_FOR_TOOLTIP) != 0)
			a_native |= ImGuiHoveredFlags_ForTooltip;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_STATIONARY) != 0)
			a_native |= ImGuiHoveredFlags_Stationary;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_DELAY_NONE) != 0)
			a_native |= ImGuiHoveredFlags_DelayNone;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_DELAY_SHORT) != 0)
			a_native |= ImGuiHoveredFlags_DelayShort;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_DELAY_NORMAL) != 0)
			a_native |= ImGuiHoveredFlags_DelayNormal;
		if ((a_value & DMUI_UI_HOVERED_FLAGS_NO_SHARED_DELAY) != 0)
			a_native |= ImGuiHoveredFlags_NoSharedDelay;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] inline DMUI_Result TranslateInputTextFlags(
		DMUI_UIInputTextFlags a_value,
		ImGuiInputTextFlags& a_native) noexcept
	{
		constexpr uint32_t known{ DMUI_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL | DMUI_UI_INPUT_TEXT_FLAGS_CHARS_HEXADECIMAL | DMUI_UI_INPUT_TEXT_FLAGS_CHARS_SCIENTIFIC | DMUI_UI_INPUT_TEXT_FLAGS_CHARS_UPPERCASE | DMUI_UI_INPUT_TEXT_FLAGS_CHARS_NO_BLANK | DMUI_UI_INPUT_TEXT_FLAGS_ALLOW_TAB_INPUT | DMUI_UI_INPUT_TEXT_FLAGS_ENTER_RETURNS_TRUE | DMUI_UI_INPUT_TEXT_FLAGS_ESCAPE_CLEARS_ALL | DMUI_UI_INPUT_TEXT_FLAGS_CTRL_ENTER_FOR_NEW_LINE | DMUI_UI_INPUT_TEXT_FLAGS_READ_ONLY | DMUI_UI_INPUT_TEXT_FLAGS_PASSWORD | DMUI_UI_INPUT_TEXT_FLAGS_ALWAYS_OVERWRITE | DMUI_UI_INPUT_TEXT_FLAGS_AUTO_SELECT_ALL | DMUI_UI_INPUT_TEXT_FLAGS_PARSE_EMPTY_REF_VAL | DMUI_UI_INPUT_TEXT_FLAGS_DISPLAY_EMPTY_REF_VAL | DMUI_UI_INPUT_TEXT_FLAGS_NO_HORIZONTAL_SCROLL | DMUI_UI_INPUT_TEXT_FLAGS_NO_UNDO_REDO | DMUI_UI_INPUT_TEXT_FLAGS_ELIDE_LEFT | DMUI_UI_INPUT_TEXT_FLAGS_WORD_WRAP };
		if ((a_value & ~known) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		a_native = 0;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL) != 0)
			a_native |= ImGuiInputTextFlags_CharsDecimal;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_CHARS_HEXADECIMAL) != 0)
			a_native |= ImGuiInputTextFlags_CharsHexadecimal;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_CHARS_SCIENTIFIC) != 0)
			a_native |= ImGuiInputTextFlags_CharsScientific;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_CHARS_UPPERCASE) != 0)
			a_native |= ImGuiInputTextFlags_CharsUppercase;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_CHARS_NO_BLANK) != 0)
			a_native |= ImGuiInputTextFlags_CharsNoBlank;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_ALLOW_TAB_INPUT) != 0)
			a_native |= ImGuiInputTextFlags_AllowTabInput;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_ENTER_RETURNS_TRUE) != 0)
			a_native |= ImGuiInputTextFlags_EnterReturnsTrue;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_ESCAPE_CLEARS_ALL) != 0)
			a_native |= ImGuiInputTextFlags_EscapeClearsAll;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_CTRL_ENTER_FOR_NEW_LINE) != 0)
			a_native |= ImGuiInputTextFlags_CtrlEnterForNewLine;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_READ_ONLY) != 0)
			a_native |= ImGuiInputTextFlags_ReadOnly;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_PASSWORD) != 0)
			a_native |= ImGuiInputTextFlags_Password;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_ALWAYS_OVERWRITE) != 0)
			a_native |= ImGuiInputTextFlags_AlwaysOverwrite;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_AUTO_SELECT_ALL) != 0)
			a_native |= ImGuiInputTextFlags_AutoSelectAll;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_PARSE_EMPTY_REF_VAL) != 0)
			a_native |= ImGuiInputTextFlags_ParseEmptyRefVal;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_DISPLAY_EMPTY_REF_VAL) != 0)
			a_native |= ImGuiInputTextFlags_DisplayEmptyRefVal;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_NO_HORIZONTAL_SCROLL) != 0)
			a_native |= ImGuiInputTextFlags_NoHorizontalScroll;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_NO_UNDO_REDO) != 0)
			a_native |= ImGuiInputTextFlags_NoUndoRedo;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_ELIDE_LEFT) != 0)
			a_native |= ImGuiInputTextFlags_ElideLeft;
		if ((a_value & DMUI_UI_INPUT_TEXT_FLAGS_WORD_WRAP) != 0)
			a_native |= ImGuiInputTextFlags_WordWrap;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] inline DMUI_Result TranslateSelectableFlags(
		DMUI_UISelectableFlags a_value,
		ImGuiSelectableFlags& a_native) noexcept
	{
		constexpr uint32_t known{ DMUI_UI_SELECTABLE_FLAGS_NO_AUTO_CLOSE_POPUPS | DMUI_UI_SELECTABLE_FLAGS_SPAN_ALL_COLUMNS | DMUI_UI_SELECTABLE_FLAGS_ALLOW_DOUBLE_CLICK | DMUI_UI_SELECTABLE_FLAGS_DISABLED | DMUI_UI_SELECTABLE_FLAGS_ALLOW_OVERLAP | DMUI_UI_SELECTABLE_FLAGS_HIGHLIGHT | DMUI_UI_SELECTABLE_FLAGS_SELECT_ON_NAV };
		if ((a_value & ~known) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		a_native = 0;
		if ((a_value & DMUI_UI_SELECTABLE_FLAGS_NO_AUTO_CLOSE_POPUPS) != 0)
			a_native |= ImGuiSelectableFlags_NoAutoClosePopups;
		if ((a_value & DMUI_UI_SELECTABLE_FLAGS_SPAN_ALL_COLUMNS) != 0)
			a_native |= ImGuiSelectableFlags_SpanAllColumns;
		if ((a_value & DMUI_UI_SELECTABLE_FLAGS_ALLOW_DOUBLE_CLICK) != 0)
			a_native |= ImGuiSelectableFlags_AllowDoubleClick;
		if ((a_value & DMUI_UI_SELECTABLE_FLAGS_DISABLED) != 0)
			a_native |= ImGuiSelectableFlags_Disabled;
		if ((a_value & DMUI_UI_SELECTABLE_FLAGS_ALLOW_OVERLAP) != 0)
			a_native |= ImGuiSelectableFlags_AllowOverlap;
		if ((a_value & DMUI_UI_SELECTABLE_FLAGS_HIGHLIGHT) != 0)
			a_native |= ImGuiSelectableFlags_Highlight;
		if ((a_value & DMUI_UI_SELECTABLE_FLAGS_SELECT_ON_NAV) != 0)
			a_native |= ImGuiSelectableFlags_SelectOnNav;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] inline DMUI_Result TranslateSliderFlags(
		DMUI_UISliderFlags a_value,
		ImGuiSliderFlags& a_native) noexcept
	{
		constexpr uint32_t known{ DMUI_UI_SLIDER_FLAGS_LOGARITHMIC | DMUI_UI_SLIDER_FLAGS_NO_ROUND_TO_FORMAT | DMUI_UI_SLIDER_FLAGS_NO_INPUT | DMUI_UI_SLIDER_FLAGS_WRAP_AROUND | DMUI_UI_SLIDER_FLAGS_CLAMP_ON_INPUT | DMUI_UI_SLIDER_FLAGS_CLAMP_ZERO_RANGE | DMUI_UI_SLIDER_FLAGS_NO_SPEED_TWEAKS | DMUI_UI_SLIDER_FLAGS_COLOR_MARKERS };
		if ((a_value & ~known) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		a_native = 0;
		if ((a_value & DMUI_UI_SLIDER_FLAGS_LOGARITHMIC) != 0)
			a_native |= ImGuiSliderFlags_Logarithmic;
		if ((a_value & DMUI_UI_SLIDER_FLAGS_NO_ROUND_TO_FORMAT) != 0)
			a_native |= ImGuiSliderFlags_NoRoundToFormat;
		if ((a_value & DMUI_UI_SLIDER_FLAGS_NO_INPUT) != 0)
			a_native |= ImGuiSliderFlags_NoInput;
		if ((a_value & DMUI_UI_SLIDER_FLAGS_WRAP_AROUND) != 0)
			a_native |= ImGuiSliderFlags_WrapAround;
		if ((a_value & DMUI_UI_SLIDER_FLAGS_CLAMP_ON_INPUT) != 0)
			a_native |= ImGuiSliderFlags_ClampOnInput;
		if ((a_value & DMUI_UI_SLIDER_FLAGS_CLAMP_ZERO_RANGE) != 0)
			a_native |= ImGuiSliderFlags_ClampZeroRange;
		if ((a_value & DMUI_UI_SLIDER_FLAGS_NO_SPEED_TWEAKS) != 0)
			a_native |= ImGuiSliderFlags_NoSpeedTweaks;
		if ((a_value & DMUI_UI_SLIDER_FLAGS_COLOR_MARKERS) != 0)
			a_native |= ImGuiSliderFlags_ColorMarkers;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] inline DMUI_Result TranslateTableFlags(
		DMUI_UITableFlags a_value,
		ImGuiTableFlags& a_native) noexcept
	{
		constexpr uint32_t known{ DMUI_UI_TABLE_FLAGS_RESIZABLE | DMUI_UI_TABLE_FLAGS_REORDERABLE | DMUI_UI_TABLE_FLAGS_HIDEABLE | DMUI_UI_TABLE_FLAGS_SORTABLE | DMUI_UI_TABLE_FLAGS_NO_SAVED_SETTINGS | DMUI_UI_TABLE_FLAGS_CONTEXT_MENU_IN_BODY | DMUI_UI_TABLE_FLAGS_ROW_BG | DMUI_UI_TABLE_FLAGS_BORDERS_INNER_H | DMUI_UI_TABLE_FLAGS_BORDERS_OUTER_H | DMUI_UI_TABLE_FLAGS_BORDERS_INNER_V | DMUI_UI_TABLE_FLAGS_BORDERS_OUTER_V | DMUI_UI_TABLE_FLAGS_NO_BORDERS_IN_BODY | DMUI_UI_TABLE_FLAGS_NO_BORDERS_IN_BODY_UNTIL_RESIZE | DMUI_UI_TABLE_FLAGS_SIZING_FIXED_FIT | DMUI_UI_TABLE_FLAGS_SIZING_FIXED_SAME | DMUI_UI_TABLE_FLAGS_SIZING_STRETCH_PROP | DMUI_UI_TABLE_FLAGS_SIZING_STRETCH_SAME | DMUI_UI_TABLE_FLAGS_NO_HOST_EXTEND_X | DMUI_UI_TABLE_FLAGS_NO_HOST_EXTEND_Y | DMUI_UI_TABLE_FLAGS_NO_KEEP_COLUMNS_VISIBLE | DMUI_UI_TABLE_FLAGS_PRECISE_WIDTHS | DMUI_UI_TABLE_FLAGS_NO_CLIP | DMUI_UI_TABLE_FLAGS_PAD_OUTER_X | DMUI_UI_TABLE_FLAGS_NO_PAD_OUTER_X | DMUI_UI_TABLE_FLAGS_NO_PAD_INNER_X | DMUI_UI_TABLE_FLAGS_SCROLL_X | DMUI_UI_TABLE_FLAGS_SCROLL_Y | DMUI_UI_TABLE_FLAGS_SORT_MULTI | DMUI_UI_TABLE_FLAGS_SORT_TRISTATE | DMUI_UI_TABLE_FLAGS_HIGHLIGHT_HOVERED_COLUMN };
		if ((a_value & ~known) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		constexpr uint32_t sizingFixedFitMask{ DMUI_UI_TABLE_FLAGS_SIZING_FIXED_FIT | DMUI_UI_TABLE_FLAGS_SIZING_FIXED_SAME | DMUI_UI_TABLE_FLAGS_SIZING_STRETCH_PROP | DMUI_UI_TABLE_FLAGS_SIZING_STRETCH_SAME };
		const auto sizingFixedFitBits = a_value & sizingFixedFitMask;
		if (sizingFixedFitBits != 0 && (sizingFixedFitBits & (sizingFixedFitBits - 1u)) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		a_native = 0;
		if ((a_value & DMUI_UI_TABLE_FLAGS_RESIZABLE) != 0)
			a_native |= ImGuiTableFlags_Resizable;
		if ((a_value & DMUI_UI_TABLE_FLAGS_REORDERABLE) != 0)
			a_native |= ImGuiTableFlags_Reorderable;
		if ((a_value & DMUI_UI_TABLE_FLAGS_HIDEABLE) != 0)
			a_native |= ImGuiTableFlags_Hideable;
		if ((a_value & DMUI_UI_TABLE_FLAGS_SORTABLE) != 0)
			a_native |= ImGuiTableFlags_Sortable;
		if ((a_value & DMUI_UI_TABLE_FLAGS_NO_SAVED_SETTINGS) != 0)
			a_native |= ImGuiTableFlags_NoSavedSettings;
		if ((a_value & DMUI_UI_TABLE_FLAGS_CONTEXT_MENU_IN_BODY) != 0)
			a_native |= ImGuiTableFlags_ContextMenuInBody;
		if ((a_value & DMUI_UI_TABLE_FLAGS_ROW_BG) != 0)
			a_native |= ImGuiTableFlags_RowBg;
		if ((a_value & DMUI_UI_TABLE_FLAGS_BORDERS_INNER_H) != 0)
			a_native |= ImGuiTableFlags_BordersInnerH;
		if ((a_value & DMUI_UI_TABLE_FLAGS_BORDERS_OUTER_H) != 0)
			a_native |= ImGuiTableFlags_BordersOuterH;
		if ((a_value & DMUI_UI_TABLE_FLAGS_BORDERS_INNER_V) != 0)
			a_native |= ImGuiTableFlags_BordersInnerV;
		if ((a_value & DMUI_UI_TABLE_FLAGS_BORDERS_OUTER_V) != 0)
			a_native |= ImGuiTableFlags_BordersOuterV;
		if ((a_value & DMUI_UI_TABLE_FLAGS_NO_BORDERS_IN_BODY) != 0)
			a_native |= ImGuiTableFlags_NoBordersInBody;
		if ((a_value & DMUI_UI_TABLE_FLAGS_NO_BORDERS_IN_BODY_UNTIL_RESIZE) != 0)
			a_native |= ImGuiTableFlags_NoBordersInBodyUntilResize;
		if ((a_value & DMUI_UI_TABLE_FLAGS_SIZING_FIXED_FIT) != 0)
			a_native |= ImGuiTableFlags_SizingFixedFit;
		if ((a_value & DMUI_UI_TABLE_FLAGS_SIZING_FIXED_SAME) != 0)
			a_native |= ImGuiTableFlags_SizingFixedSame;
		if ((a_value & DMUI_UI_TABLE_FLAGS_SIZING_STRETCH_PROP) != 0)
			a_native |= ImGuiTableFlags_SizingStretchProp;
		if ((a_value & DMUI_UI_TABLE_FLAGS_SIZING_STRETCH_SAME) != 0)
			a_native |= ImGuiTableFlags_SizingStretchSame;
		if ((a_value & DMUI_UI_TABLE_FLAGS_NO_HOST_EXTEND_X) != 0)
			a_native |= ImGuiTableFlags_NoHostExtendX;
		if ((a_value & DMUI_UI_TABLE_FLAGS_NO_HOST_EXTEND_Y) != 0)
			a_native |= ImGuiTableFlags_NoHostExtendY;
		if ((a_value & DMUI_UI_TABLE_FLAGS_NO_KEEP_COLUMNS_VISIBLE) != 0)
			a_native |= ImGuiTableFlags_NoKeepColumnsVisible;
		if ((a_value & DMUI_UI_TABLE_FLAGS_PRECISE_WIDTHS) != 0)
			a_native |= ImGuiTableFlags_PreciseWidths;
		if ((a_value & DMUI_UI_TABLE_FLAGS_NO_CLIP) != 0)
			a_native |= ImGuiTableFlags_NoClip;
		if ((a_value & DMUI_UI_TABLE_FLAGS_PAD_OUTER_X) != 0)
			a_native |= ImGuiTableFlags_PadOuterX;
		if ((a_value & DMUI_UI_TABLE_FLAGS_NO_PAD_OUTER_X) != 0)
			a_native |= ImGuiTableFlags_NoPadOuterX;
		if ((a_value & DMUI_UI_TABLE_FLAGS_NO_PAD_INNER_X) != 0)
			a_native |= ImGuiTableFlags_NoPadInnerX;
		if ((a_value & DMUI_UI_TABLE_FLAGS_SCROLL_X) != 0)
			a_native |= ImGuiTableFlags_ScrollX;
		if ((a_value & DMUI_UI_TABLE_FLAGS_SCROLL_Y) != 0)
			a_native |= ImGuiTableFlags_ScrollY;
		if ((a_value & DMUI_UI_TABLE_FLAGS_SORT_MULTI) != 0)
			a_native |= ImGuiTableFlags_SortMulti;
		if ((a_value & DMUI_UI_TABLE_FLAGS_SORT_TRISTATE) != 0)
			a_native |= ImGuiTableFlags_SortTristate;
		if ((a_value & DMUI_UI_TABLE_FLAGS_HIGHLIGHT_HOVERED_COLUMN) != 0)
			a_native |= ImGuiTableFlags_HighlightHoveredColumn;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] inline DMUI_Result TranslateTableColumnFlags(
		DMUI_UITableColumnFlags a_value,
		ImGuiTableColumnFlags& a_native) noexcept
	{
		constexpr uint32_t known{ DMUI_UI_TABLE_COLUMN_FLAGS_DISABLED | DMUI_UI_TABLE_COLUMN_FLAGS_DEFAULT_HIDE | DMUI_UI_TABLE_COLUMN_FLAGS_DEFAULT_SORT | DMUI_UI_TABLE_COLUMN_FLAGS_WIDTH_STRETCH | DMUI_UI_TABLE_COLUMN_FLAGS_WIDTH_FIXED | DMUI_UI_TABLE_COLUMN_FLAGS_NO_RESIZE | DMUI_UI_TABLE_COLUMN_FLAGS_NO_REORDER | DMUI_UI_TABLE_COLUMN_FLAGS_NO_HIDE | DMUI_UI_TABLE_COLUMN_FLAGS_NO_CLIP | DMUI_UI_TABLE_COLUMN_FLAGS_NO_SORT | DMUI_UI_TABLE_COLUMN_FLAGS_NO_SORT_ASCENDING | DMUI_UI_TABLE_COLUMN_FLAGS_NO_SORT_DESCENDING | DMUI_UI_TABLE_COLUMN_FLAGS_NO_HEADER_LABEL | DMUI_UI_TABLE_COLUMN_FLAGS_NO_HEADER_WIDTH | DMUI_UI_TABLE_COLUMN_FLAGS_PREFER_SORT_ASCENDING | DMUI_UI_TABLE_COLUMN_FLAGS_PREFER_SORT_DESCENDING | DMUI_UI_TABLE_COLUMN_FLAGS_INDENT_ENABLE | DMUI_UI_TABLE_COLUMN_FLAGS_INDENT_DISABLE | DMUI_UI_TABLE_COLUMN_FLAGS_ANGLED_HEADER };
		if ((a_value & ~known) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		constexpr uint32_t widthStretchMask{ DMUI_UI_TABLE_COLUMN_FLAGS_WIDTH_STRETCH | DMUI_UI_TABLE_COLUMN_FLAGS_WIDTH_FIXED };
		const auto widthStretchBits = a_value & widthStretchMask;
		if (widthStretchBits != 0 && (widthStretchBits & (widthStretchBits - 1u)) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		constexpr uint32_t preferSortAscendingMask{ DMUI_UI_TABLE_COLUMN_FLAGS_PREFER_SORT_ASCENDING | DMUI_UI_TABLE_COLUMN_FLAGS_PREFER_SORT_DESCENDING };
		const auto preferSortAscendingBits = a_value & preferSortAscendingMask;
		if (preferSortAscendingBits != 0 && (preferSortAscendingBits & (preferSortAscendingBits - 1u)) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		constexpr uint32_t indentEnableMask{ DMUI_UI_TABLE_COLUMN_FLAGS_INDENT_ENABLE | DMUI_UI_TABLE_COLUMN_FLAGS_INDENT_DISABLE };
		const auto indentEnableBits = a_value & indentEnableMask;
		if (indentEnableBits != 0 && (indentEnableBits & (indentEnableBits - 1u)) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		a_native = 0;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_DISABLED) != 0)
			a_native |= ImGuiTableColumnFlags_Disabled;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_DEFAULT_HIDE) != 0)
			a_native |= ImGuiTableColumnFlags_DefaultHide;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_DEFAULT_SORT) != 0)
			a_native |= ImGuiTableColumnFlags_DefaultSort;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_WIDTH_STRETCH) != 0)
			a_native |= ImGuiTableColumnFlags_WidthStretch;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_WIDTH_FIXED) != 0)
			a_native |= ImGuiTableColumnFlags_WidthFixed;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_NO_RESIZE) != 0)
			a_native |= ImGuiTableColumnFlags_NoResize;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_NO_REORDER) != 0)
			a_native |= ImGuiTableColumnFlags_NoReorder;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_NO_HIDE) != 0)
			a_native |= ImGuiTableColumnFlags_NoHide;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_NO_CLIP) != 0)
			a_native |= ImGuiTableColumnFlags_NoClip;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_NO_SORT) != 0)
			a_native |= ImGuiTableColumnFlags_NoSort;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_NO_SORT_ASCENDING) != 0)
			a_native |= ImGuiTableColumnFlags_NoSortAscending;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_NO_SORT_DESCENDING) != 0)
			a_native |= ImGuiTableColumnFlags_NoSortDescending;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_NO_HEADER_LABEL) != 0)
			a_native |= ImGuiTableColumnFlags_NoHeaderLabel;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_NO_HEADER_WIDTH) != 0)
			a_native |= ImGuiTableColumnFlags_NoHeaderWidth;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_PREFER_SORT_ASCENDING) != 0)
			a_native |= ImGuiTableColumnFlags_PreferSortAscending;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_PREFER_SORT_DESCENDING) != 0)
			a_native |= ImGuiTableColumnFlags_PreferSortDescending;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_INDENT_ENABLE) != 0)
			a_native |= ImGuiTableColumnFlags_IndentEnable;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_INDENT_DISABLE) != 0)
			a_native |= ImGuiTableColumnFlags_IndentDisable;
		if ((a_value & DMUI_UI_TABLE_COLUMN_FLAGS_ANGLED_HEADER) != 0)
			a_native |= ImGuiTableColumnFlags_AngledHeader;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] inline DMUI_Result TranslateTableRowFlags(
		DMUI_UITableRowFlags a_value,
		ImGuiTableRowFlags& a_native) noexcept
	{
		constexpr uint32_t known{ DMUI_UI_TABLE_ROW_FLAGS_HEADERS };
		if ((a_value & ~known) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		a_native = 0;
		if ((a_value & DMUI_UI_TABLE_ROW_FLAGS_HEADERS) != 0)
			a_native |= ImGuiTableRowFlags_Headers;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] inline DMUI_Result TranslateTreeNodeFlags(
		DMUI_UITreeNodeFlags a_value,
		ImGuiTreeNodeFlags& a_native) noexcept
	{
		constexpr uint32_t known{ DMUI_UI_TREE_NODE_FLAGS_SELECTED | DMUI_UI_TREE_NODE_FLAGS_FRAMED | DMUI_UI_TREE_NODE_FLAGS_ALLOW_OVERLAP | DMUI_UI_TREE_NODE_FLAGS_NO_TREE_PUSH_ON_OPEN | DMUI_UI_TREE_NODE_FLAGS_NO_AUTO_OPEN_ON_LOG | DMUI_UI_TREE_NODE_FLAGS_DEFAULT_OPEN | DMUI_UI_TREE_NODE_FLAGS_OPEN_ON_DOUBLE_CLICK | DMUI_UI_TREE_NODE_FLAGS_OPEN_ON_ARROW | DMUI_UI_TREE_NODE_FLAGS_LEAF | DMUI_UI_TREE_NODE_FLAGS_BULLET | DMUI_UI_TREE_NODE_FLAGS_FRAME_PADDING | DMUI_UI_TREE_NODE_FLAGS_SPAN_AVAIL_WIDTH | DMUI_UI_TREE_NODE_FLAGS_SPAN_FULL_WIDTH | DMUI_UI_TREE_NODE_FLAGS_SPAN_LABEL_WIDTH | DMUI_UI_TREE_NODE_FLAGS_SPAN_ALL_COLUMNS | DMUI_UI_TREE_NODE_FLAGS_LABEL_SPAN_ALL_COLUMNS | DMUI_UI_TREE_NODE_FLAGS_NAV_LEFT_JUMPS_TO_PARENT | DMUI_UI_TREE_NODE_FLAGS_DRAW_LINES_NONE | DMUI_UI_TREE_NODE_FLAGS_DRAW_LINES_FULL | DMUI_UI_TREE_NODE_FLAGS_DRAW_LINES_TO_NODES };
		if ((a_value & ~known) != 0)
			return DMUI_RESULT_INVALID_ARGUMENT;
		a_native = 0;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_SELECTED) != 0)
			a_native |= ImGuiTreeNodeFlags_Selected;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_FRAMED) != 0)
			a_native |= ImGuiTreeNodeFlags_Framed;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_ALLOW_OVERLAP) != 0)
			a_native |= ImGuiTreeNodeFlags_AllowOverlap;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_NO_TREE_PUSH_ON_OPEN) != 0)
			a_native |= ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_NO_AUTO_OPEN_ON_LOG) != 0)
			a_native |= ImGuiTreeNodeFlags_NoAutoOpenOnLog;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_DEFAULT_OPEN) != 0)
			a_native |= ImGuiTreeNodeFlags_DefaultOpen;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_OPEN_ON_DOUBLE_CLICK) != 0)
			a_native |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_OPEN_ON_ARROW) != 0)
			a_native |= ImGuiTreeNodeFlags_OpenOnArrow;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_LEAF) != 0)
			a_native |= ImGuiTreeNodeFlags_Leaf;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_BULLET) != 0)
			a_native |= ImGuiTreeNodeFlags_Bullet;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_FRAME_PADDING) != 0)
			a_native |= ImGuiTreeNodeFlags_FramePadding;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_SPAN_AVAIL_WIDTH) != 0)
			a_native |= ImGuiTreeNodeFlags_SpanAvailWidth;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_SPAN_FULL_WIDTH) != 0)
			a_native |= ImGuiTreeNodeFlags_SpanFullWidth;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_SPAN_LABEL_WIDTH) != 0)
			a_native |= ImGuiTreeNodeFlags_SpanLabelWidth;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_SPAN_ALL_COLUMNS) != 0)
			a_native |= ImGuiTreeNodeFlags_SpanAllColumns;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_LABEL_SPAN_ALL_COLUMNS) != 0)
			a_native |= ImGuiTreeNodeFlags_LabelSpanAllColumns;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_NAV_LEFT_JUMPS_TO_PARENT) != 0)
			a_native |= ImGuiTreeNodeFlags_NavLeftJumpsToParent;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_DRAW_LINES_NONE) != 0)
			a_native |= ImGuiTreeNodeFlags_DrawLinesNone;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_DRAW_LINES_FULL) != 0)
			a_native |= ImGuiTreeNodeFlags_DrawLinesFull;
		if ((a_value & DMUI_UI_TREE_NODE_FLAGS_DRAW_LINES_TO_NODES) != 0)
			a_native |= ImGuiTreeNodeFlags_DrawLinesToNodes;
		return DMUI_RESULT_OK;
	}

	[[nodiscard]] DMUI_Result DMUI_CALL GetStyleMetrics(
		DMUI_ClientHandle a_client,
		DMUI_StyleMetrics* a_metrics) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL BeginCombo(
		DMUI_ClientHandle a_client,
		const char* a_label,
		const char* a_previewValue,
		DMUI_UIComboFlags a_flags,
		uint32_t* a_visible) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL EndCombo(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL BeginDisabled(
		DMUI_ClientHandle a_client,
		uint32_t a_disabled) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL EndDisabled(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL BeginTable(
		DMUI_ClientHandle a_client,
		const char* a_id,
		int32_t a_columns,
		DMUI_UITableFlags a_flags,
		DMUI_Vec2 a_outerSize,
		float a_innerWidth,
		uint32_t* a_visible) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL EndTable(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL BeginTooltip(
		DMUI_ClientHandle a_client,
		uint32_t* a_visible) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL EndTooltip(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL Button(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_Vec2 a_size,
		uint32_t* a_pressed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL CalcTextSize(
		DMUI_ClientHandle a_client,
		const char* a_text,
		size_t a_textLength,
		uint32_t a_hideTextAfterDoubleHash,
		float a_wrapWidth,
		DMUI_Vec2* a_size) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL Checkbox(
		DMUI_ClientHandle a_client,
		const char* a_label,
		uint32_t* a_value,
		uint32_t* a_changed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL CollapsingHeader(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_UITreeNodeFlags a_flags,
		uint32_t* a_open) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL CollapsingHeaderVisible(
		DMUI_ClientHandle a_client,
		const char* a_label,
		uint32_t* a_visible,
		DMUI_UITreeNodeFlags a_flags,
		uint32_t* a_open) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL DragScalar(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_UIDataType a_dataType,
		void* a_data,
		uint32_t a_dataSize,
		float a_speed,
		const void* a_minimum,
		uint32_t a_minimumSize,
		const void* a_maximum,
		uint32_t a_maximumSize,
		const char* a_format,
		DMUI_UISliderFlags a_flags,
		uint32_t* a_changed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL Dummy(
		DMUI_ClientHandle a_client,
		DMUI_Vec2 a_size) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL GetContentRegionAvail(
		DMUI_ClientHandle a_client,
		DMUI_Vec2* a_size) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL GetCursorScreenPos(
		DMUI_ClientHandle a_client,
		DMUI_Vec2* a_position) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL GetFontSize(
		DMUI_ClientHandle a_client,
		float* a_size) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL GetFrameHeight(
		DMUI_ClientHandle a_client,
		float* a_height) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL GetStyleColor(
		DMUI_ClientHandle a_client,
		DMUI_UIColor a_color,
		DMUI_Vec4* a_value) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL GetTextLineHeightWithSpacing(
		DMUI_ClientHandle a_client,
		float* a_height) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL Indent(
		DMUI_ClientHandle a_client,
		float a_width) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL InputScalar(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_UIDataType a_dataType,
		void* a_data,
		uint32_t a_dataSize,
		const void* a_step,
		uint32_t a_stepSize,
		const void* a_fastStep,
		uint32_t a_fastStepSize,
		const char* a_format,
		DMUI_UIInputTextFlags a_flags,
		uint32_t* a_changed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL InputText(
		DMUI_ClientHandle a_client,
		const char* a_label,
		char* a_buffer,
		uint32_t a_capacity,
		DMUI_UIInputTextFlags a_flags,
		uint32_t* a_changed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL InputTextMultiline(
		DMUI_ClientHandle a_client,
		const char* a_label,
		char* a_buffer,
		uint32_t a_capacity,
		DMUI_Vec2 a_size,
		DMUI_UIInputTextFlags a_flags,
		uint32_t* a_changed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL InputTextWithHint(
		DMUI_ClientHandle a_client,
		const char* a_label,
		const char* a_hint,
		char* a_buffer,
		uint32_t a_capacity,
		DMUI_UIInputTextFlags a_flags,
		uint32_t* a_changed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL IsItemDeactivatedAfterEdit(
		DMUI_ClientHandle a_client,
		uint32_t* a_deactivated) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL IsItemHovered(
		DMUI_ClientHandle a_client,
		DMUI_UIHoveredFlags a_flags,
		uint32_t* a_hovered) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PopID(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PopStyleColor(
		DMUI_ClientHandle a_client,
		int32_t a_count) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PopTextWrapPos(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL ProgressBar(
		DMUI_ClientHandle a_client,
		float a_fraction,
		DMUI_Vec2 a_size,
		const char* a_overlay,
		size_t a_overlayLength) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PushIDString(
		DMUI_ClientHandle a_client,
		const char* a_id) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PushIDRange(
		DMUI_ClientHandle a_client,
		const char* a_id,
		size_t a_length) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PushIDValue(
		DMUI_ClientHandle a_client,
		uint64_t a_id) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PushStyleColorU32(
		DMUI_ClientHandle a_client,
		DMUI_UIColor a_color,
		uint32_t a_rgba) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PushStyleColor(
		DMUI_ClientHandle a_client,
		DMUI_UIColor a_color,
		DMUI_Vec4 a_value) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PushTextWrapPos(
		DMUI_ClientHandle a_client,
		float a_localX) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL SameLine(
		DMUI_ClientHandle a_client,
		float a_offsetFromStartX,
		float a_spacing) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL Selectable(
		DMUI_ClientHandle a_client,
		const char* a_label,
		uint32_t a_selected,
		DMUI_UISelectableFlags a_flags,
		DMUI_Vec2 a_size,
		uint32_t* a_pressed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL SelectableToggle(
		DMUI_ClientHandle a_client,
		const char* a_label,
		uint32_t* a_selected,
		DMUI_UISelectableFlags a_flags,
		DMUI_Vec2 a_size,
		uint32_t* a_pressed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL Separator(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL SetClipboardText(
		DMUI_ClientHandle a_client,
		const char* a_text,
		size_t a_textLength) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL SetCursorScreenPos(
		DMUI_ClientHandle a_client,
		DMUI_Vec2 a_position) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL SetItemDefaultFocus(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL SetNextItemWidth(
		DMUI_ClientHandle a_client,
		float a_width) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL SetTooltipText(
		DMUI_ClientHandle a_client,
		const char* a_text,
		size_t a_textLength) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL SliderScalar(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_UIDataType a_dataType,
		void* a_data,
		uint32_t a_dataSize,
		const void* a_minimum,
		uint32_t a_minimumSize,
		const void* a_maximum,
		uint32_t a_maximumSize,
		const char* a_format,
		DMUI_UISliderFlags a_flags,
		uint32_t* a_changed) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL Spacing(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL TableHeadersRow(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL TableNextColumn(
		DMUI_ClientHandle a_client,
		uint32_t* a_visible) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL TableNextRow(
		DMUI_ClientHandle a_client,
		DMUI_UITableRowFlags a_flags,
		float a_minimumHeight) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL TableSetColumnIndex(
		DMUI_ClientHandle a_client,
		int32_t a_column,
		uint32_t* a_visible) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL TableSetupColumn(
		DMUI_ClientHandle a_client,
		const char* a_label,
		DMUI_UITableColumnFlags a_flags,
		float a_initialWidthOrWeight,
		uint32_t a_userId) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL TableSetupScrollFreeze(
		DMUI_ClientHandle a_client,
		int32_t a_columns,
		int32_t a_rows) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL Text(
		DMUI_ClientHandle a_client,
		const char* a_text,
		size_t a_textLength) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL TextColored(
		DMUI_ClientHandle a_client,
		DMUI_Vec4 a_color,
		const char* a_text,
		size_t a_textLength) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL TextDisabled(
		DMUI_ClientHandle a_client,
		const char* a_text,
		size_t a_textLength) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL TextWrapped(
		DMUI_ClientHandle a_client,
		const char* a_text,
		size_t a_textLength) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL Unindent(
		DMUI_ClientHandle a_client,
		float a_width) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL NewLine(
		DMUI_ClientHandle a_client) noexcept;
	[[nodiscard]] DMUI_Result DMUI_CALL PlotLines(
		DMUI_ClientHandle a_client,
		const char* a_label,
		const float* a_values,
		int32_t a_valueCount,
		int32_t a_valueOffset,
		const char* a_overlay,
		size_t a_overlayLength,
		float a_scaleMinimum,
		float a_scaleMaximum,
		DMUI_Vec2 a_size,
		uint32_t a_strideBytes) noexcept;

	[[nodiscard]] inline DMUI_UIAPI MakeAPI() noexcept
	{
		return {
			DMUI_UI_API_CURRENT_SIZE,
			DMUI_UI_ABI_CURRENT,
			DMUI_UI_REVISION_CURRENT,
			0u,
			&GetStyleMetrics,
			&BeginCombo,
			&EndCombo,
			&BeginDisabled,
			&EndDisabled,
			&BeginTable,
			&EndTable,
			&BeginTooltip,
			&EndTooltip,
			&Button,
			&CalcTextSize,
			&Checkbox,
			&CollapsingHeader,
			&CollapsingHeaderVisible,
			&DragScalar,
			&Dummy,
			&GetContentRegionAvail,
			&GetCursorScreenPos,
			&GetFontSize,
			&GetFrameHeight,
			&GetStyleColor,
			&GetTextLineHeightWithSpacing,
			&Indent,
			&InputScalar,
			&InputText,
			&InputTextMultiline,
			&InputTextWithHint,
			&IsItemDeactivatedAfterEdit,
			&IsItemHovered,
			&PopID,
			&PopStyleColor,
			&PopTextWrapPos,
			&ProgressBar,
			&PushIDString,
			&PushIDRange,
			&PushIDValue,
			&PushStyleColorU32,
			&PushStyleColor,
			&PushTextWrapPos,
			&SameLine,
			&Selectable,
			&SelectableToggle,
			&Separator,
			&SetClipboardText,
			&SetCursorScreenPos,
			&SetItemDefaultFocus,
			&SetNextItemWidth,
			&SetTooltipText,
			&SliderScalar,
			&Spacing,
			&TableHeadersRow,
			&TableNextColumn,
			&TableNextRow,
			&TableSetColumnIndex,
			&TableSetupColumn,
			&TableSetupScrollFreeze,
			&Text,
			&TextColored,
			&TextDisabled,
			&TextWrapped,
			&Unindent,
			&NewLine,
			&PlotLines
		};
	}
}
