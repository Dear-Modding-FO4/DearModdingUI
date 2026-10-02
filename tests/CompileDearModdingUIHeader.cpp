#include <DearModdingUI/API.h>

#include <type_traits>

static_assert(std::is_standard_layout_v<DMUI_HostReadyInfo>);
static_assert(std::is_trivially_copyable_v<DMUI_HostReadyInfo>);
static_assert(std::is_standard_layout_v<DMUI_ClientDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_ClientDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_PageDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_PageDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_CategoryDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_CategoryDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_ActionDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_ActionDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_FrameObserverDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_FrameObserverDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_HotkeyActionDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_HotkeyActionDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_LinkDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_LinkDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_ExternalOpenDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_ExternalOpenDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_FaqEntry>);
static_assert(std::is_trivially_copyable_v<DMUI_FaqEntry>);
static_assert(std::is_standard_layout_v<DMUI_DiagnosticDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_DiagnosticDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_HotkeyBindingInfo>);
static_assert(std::is_trivially_copyable_v<DMUI_HotkeyBindingInfo>);
static_assert(std::is_standard_layout_v<DMUI_Vec2>);
static_assert(std::is_trivially_copyable_v<DMUI_Vec2>);
static_assert(std::is_standard_layout_v<DMUI_Vec4>);
static_assert(std::is_trivially_copyable_v<DMUI_Vec4>);
static_assert(std::is_standard_layout_v<DMUI_FieldBeginOptions>);
static_assert(std::is_trivially_copyable_v<DMUI_FieldBeginOptions>);
static_assert(std::is_standard_layout_v<DMUI_FieldEndOptions>);
static_assert(std::is_trivially_copyable_v<DMUI_FieldEndOptions>);
static_assert(std::is_standard_layout_v<DMUI_FieldFeedback>);
static_assert(std::is_trivially_copyable_v<DMUI_FieldFeedback>);
static_assert(std::is_standard_layout_v<DMUI_ThemeColors>);
static_assert(std::is_trivially_copyable_v<DMUI_ThemeColors>);
static_assert(std::is_standard_layout_v<DMUI_TextViewDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_TextViewDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_TextViewState>);
static_assert(std::is_trivially_copyable_v<DMUI_TextViewState>);
static_assert(std::is_standard_layout_v<DMUI_TextBuffer>);
static_assert(std::is_trivially_copyable_v<DMUI_TextBuffer>);
static_assert(std::is_standard_layout_v<DMUI_HostAPI>);
static_assert(std::is_trivially_copyable_v<DMUI_HostAPI>);
static_assert(sizeof(DMUI_StatusSeverity) == sizeof(uint32_t));
static_assert(sizeof(DMUI_FontRole) == sizeof(uint32_t));
static_assert(sizeof(DMUI_SettingsAction) == sizeof(uint32_t));
static_assert(sizeof(DMUI_HotkeyBindingState) == sizeof(uint32_t));
static_assert(std::is_same_v<
	decltype(&DMUI_GetAPI),
	const DMUI_HostAPI* (DMUI_CALL*)(uint32_t) noexcept>);
static_assert(!std::is_nothrow_invocable_v<
	DMUI_HostReadyCallback,
	const DMUI_HostReadyInfo*,
	void*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_AttachSwapChainFn,
	DMUI_ClientHandle,
	void*>);
static_assert(!std::is_nothrow_invocable_v<
	DMUI_ActionCallback,
	void*>);
static_assert(!std::is_nothrow_invocable_v<
	DMUI_FrameCallback,
	void*>);
static_assert(!std::is_nothrow_invocable_v<
	DMUI_HotkeyCallback,
	DMUI_HotkeyActionHandle,
	uint32_t,
	void*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_RegisterCategoryFn,
	DMUI_ClientHandle,
	const DMUI_CategoryDescriptor*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_RegisterActionFn,
	DMUI_ClientHandle,
	const DMUI_ActionDescriptor*,
	DMUI_ActionHandle*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_SetStatusFn,
	DMUI_ClientHandle,
	DMUI_StatusSeverity,
	const char*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_PushFontFn,
	DMUI_ClientHandle,
	DMUI_FontRole>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_PopFontFn,
	DMUI_ClientHandle>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_DrawSectionHeaderFn,
	DMUI_ClientHandle,
	const char*,
	uint32_t>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_DrawBulletTextFn,
	DMUI_ClientHandle,
	const char*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_DrawSearchInputFn,
	DMUI_ClientHandle,
	const char*,
	const char*,
	char*,
	size_t,
	uint32_t*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_DrawSearchInputBufferFn,
	DMUI_ClientHandle,
	const char*,
	const char*,
	DMUI_TextBuffer*,
	uint32_t*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_DrawTextViewFn,
	DMUI_ClientHandle,
	const DMUI_TextViewDescriptor*,
	DMUI_TextViewState*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_DrawCollapsingSectionHeaderFn,
	DMUI_ClientHandle,
	const char*,
	const char*,
	uint32_t,
	uint32_t*,
	size_t>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_DrawLinkRowFn,
	DMUI_ClientHandle,
	const char*,
	const DMUI_LinkDescriptor*,
	size_t>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_DrawFaqFn,
	DMUI_ClientHandle,
	const char*,
	const DMUI_FaqEntry*,
	size_t>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_ReportDiagnosticFn,
	DMUI_ClientHandle,
	const DMUI_DiagnosticDescriptor*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_DrawSettingsActionButtonFn,
	DMUI_ClientHandle,
	const char*,
	DMUI_Vec2,
	DMUI_Vec2,
	DMUI_SettingsAction,
	const char*,
	const char*,
	uint32_t,
	uint32_t*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_SettingsActionButtonWidthFn,
	DMUI_ClientHandle,
	DMUI_SettingsAction,
	const char*,
	float,
	float*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_SettingsActionButtonExtentFn,
	DMUI_ClientHandle,
	float*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_RegisterFrameObserverFn,
	DMUI_ClientHandle,
	const DMUI_FrameObserverDescriptor*,
	DMUI_FrameObserverHandle*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_QueryVideoMemoryFn,
	DMUI_ClientHandle,
	uint64_t*,
	uint64_t*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_RegisterHotkeyActionFn,
	DMUI_ClientHandle,
	const DMUI_HotkeyActionDescriptor*,
	DMUI_HotkeyActionHandle*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_QueryHotkeyBindingFn,
	DMUI_ClientHandle,
	DMUI_HotkeyActionHandle,
	DMUI_HotkeyBindingInfo*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_BeginSettingsTableFn,
	DMUI_ClientHandle,
	const char*,
	uint32_t*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_EndSettingsTableFn,
	DMUI_ClientHandle>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_BeginFieldFn,
	DMUI_ClientHandle,
	const char*,
	const char*,
	const char*,
	const DMUI_FieldBeginOptions*,
	uint32_t*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_EndFieldFn,
	DMUI_ClientHandle,
	const DMUI_FieldEndOptions*,
	uint32_t*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_OpenExternalFn,
	DMUI_ClientHandle,
	const DMUI_ExternalOpenDescriptor*,
	uint32_t*>);
static_assert(DMUI_PAGE_KIND_SETTINGS == 1u);
static_assert(DMUI_PAGE_KIND_OVERLAY == 2u);
static_assert(DMUI_STATUS_SEVERITY_INFO == 0u);
static_assert(DMUI_STATUS_SEVERITY_SUCCESS == 1u);
static_assert(DMUI_STATUS_SEVERITY_WARNING == 2u);
static_assert(DMUI_STATUS_SEVERITY_ERROR == 3u);
static_assert(DMUI_FONT_ROLE_BODY == 0u);
static_assert(DMUI_FONT_ROLE_TITLE == 1u);
static_assert(DMUI_FONT_ROLE_HEADING == 2u);
static_assert(DMUI_FONT_ROLE_SUBHEADING == 3u);
static_assert(DMUI_FONT_ROLE_SUBTEXT == 4u);
static_assert(DMUI_FONT_ROLE_MONOSPACE == 5u);
static_assert(DMUI_FONT_ROLE_COUNT == 6u);
static_assert(DMUI_SETTINGS_ACTION_RESET == 0u);
static_assert(DMUI_SETTINGS_ACTION_REVERT == 1u);
static_assert(DMUI_SETTINGS_ACTION_APPLY == 2u);
static_assert(DMUI_HOTKEY_BINDING_BOUND == 0u);
static_assert(DMUI_HOTKEY_BINDING_UNBOUND_USER == 1u);
static_assert(DMUI_HOTKEY_BINDING_UNBOUND_DEFAULT_CONFLICT == 2u);
static_assert(DMUI_HOTKEY_BINDING_UNBOUND_NEVER_SET == 3u);
static_assert(DMUI_HOTKEY_BINDING_UNBOUND_OVERRIDE_CONFLICT == 4u);
static_assert(DMUI_HOTKEY_BINDING_UNBOUND_INVALID_OVERRIDE == 5u);
static_assert(DMUI_CLIENT_ORIGIN_NATIVE == 0u);
static_assert(DMUI_CLIENT_ORIGIN_BRIDGED == 1u);

#if UINTPTR_MAX == UINT64_MAX
static_assert(sizeof(DMUI_Vec2) == 8);
static_assert(sizeof(DMUI_Vec4) == 16);
#endif
