#include <DearModdingUI/API.h>

#include <type_traits>

static_assert(std::is_standard_layout_v<DMUI_ImGuiFingerprint>);
static_assert(std::is_trivially_copyable_v<DMUI_ImGuiFingerprint>);
static_assert(std::is_standard_layout_v<DMUI_ClientDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_ClientDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_PageDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_PageDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_ActionDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_ActionDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_FrameObserverDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_FrameObserverDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_HotkeyActionDescriptor>);
static_assert(std::is_trivially_copyable_v<DMUI_HotkeyActionDescriptor>);
static_assert(std::is_standard_layout_v<DMUI_HotkeyBindingInfo>);
static_assert(std::is_trivially_copyable_v<DMUI_HotkeyBindingInfo>);
static_assert(std::is_standard_layout_v<DMUI_Vec2>);
static_assert(std::is_trivially_copyable_v<DMUI_Vec2>);
static_assert(std::is_standard_layout_v<DMUI_Vec4>);
static_assert(std::is_trivially_copyable_v<DMUI_Vec4>);
static_assert(std::is_standard_layout_v<DMUI_SettingsRowOptions>);
static_assert(std::is_trivially_copyable_v<DMUI_SettingsRowOptions>);
static_assert(std::is_standard_layout_v<DMUI_ThemeColors>);
static_assert(std::is_trivially_copyable_v<DMUI_ThemeColors>);
static_assert(std::is_standard_layout_v<DMUI_HostAPI>);
static_assert(std::is_trivially_copyable_v<DMUI_HostAPI>);
static_assert(sizeof(DMUI_StatusSeverity) == sizeof(uint32_t));
static_assert(sizeof(DMUI_FontRole) == sizeof(uint32_t));
static_assert(sizeof(DMUI_SettingsAction) == sizeof(uint32_t));
static_assert(sizeof(DMUI_HotkeyBindingState) == sizeof(uint32_t));
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
	DMUI_GetThemeColorsFn,
	DMUI_ClientHandle,
	DMUI_ThemeColors*>);
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
	DMUI_DrawCollapsingSectionHeaderFn,
	DMUI_ClientHandle,
	const char*,
	const char*,
	uint32_t,
	uint32_t*,
	size_t>);
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
	DMUI_BeginSettingsRowFn,
	DMUI_ClientHandle,
	const char*,
	const char*,
	const char*,
	uint32_t*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_EndSettingsRowFn,
	DMUI_ClientHandle,
	const DMUI_SettingsRowOptions*,
	uint32_t*>);
static_assert(std::is_nothrow_invocable_v<
	DMUI_EndSettingsTableFn,
	DMUI_ClientHandle>);
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
static_assert(DMUI_FONT_ROLE_COUNT == 5u);
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
static_assert(sizeof(DMUI_ImGuiFingerprint) == 216);
static_assert(sizeof(DMUI_HostReadyInfo) == 40);
static_assert(sizeof(DMUI_ClientDescriptor) == 96);
static_assert(sizeof(DMUI_PageDescriptor) == 64);
static_assert(sizeof(DMUI_ActionDescriptor) == 64);
static_assert(sizeof(DMUI_FrameObserverDescriptor) == 24);
static_assert(sizeof(DMUI_HotkeyActionDescriptor) == 48);
static_assert(sizeof(DMUI_HotkeyBindingInfo) == 40);
static_assert(sizeof(DMUI_HostStateInfo) == 28);
static_assert(sizeof(DMUI_Vec2) == 8);
static_assert(sizeof(DMUI_Vec4) == 16);
static_assert(sizeof(DMUI_SettingsRowOptions) == 12);
static_assert(sizeof(DMUI_ThemeColors) == 228);
static_assert(offsetof(DMUI_Vec2, x) == 0);
static_assert(offsetof(DMUI_Vec2, y) == 4);
static_assert(offsetof(DMUI_Vec4, x) == 0);
static_assert(offsetof(DMUI_Vec4, y) == 4);
static_assert(offsetof(DMUI_Vec4, z) == 8);
static_assert(offsetof(DMUI_Vec4, w) == 12);
static_assert(offsetof(DMUI_ClientDescriptor, structSize) == 0);
static_assert(offsetof(DMUI_ClientDescriptor, capabilities) == 64);
static_assert(offsetof(DMUI_ClientDescriptor, iconName) == 72);
static_assert(offsetof(DMUI_ClientDescriptor, origin) == 80);
static_assert(offsetof(DMUI_ClientDescriptor, bridgeSourceLabel) == 88);
static_assert(DMUI_CLIENT_DESCRIPTOR_0_1_SIZE ==
	sizeof(DMUI_ClientDescriptor));
static_assert(DMUI_PAGE_DESCRIPTOR_0_1_SIZE ==
	sizeof(DMUI_PageDescriptor));
static_assert(DMUI_ACTION_DESCRIPTOR_0_1_SIZE ==
	sizeof(DMUI_ActionDescriptor));
static_assert(DMUI_FRAME_OBSERVER_DESCRIPTOR_0_1_SIZE ==
	sizeof(DMUI_FrameObserverDescriptor));
static_assert(DMUI_HOTKEY_ACTION_DESCRIPTOR_0_1_SIZE ==
	sizeof(DMUI_HotkeyActionDescriptor));
static_assert(offsetof(DMUI_ThemeColors, structSize) == 0);
static_assert(offsetof(DMUI_ThemeColors, success) == 4);
static_assert(offsetof(DMUI_ThemeColors, warning) == 20);
static_assert(offsetof(DMUI_ThemeColors, error) == 36);
static_assert(offsetof(DMUI_ThemeColors, info) == 52);
static_assert(offsetof(DMUI_ThemeColors, muted) == 68);
static_assert(offsetof(DMUI_ThemeColors, accent) == 84);
static_assert(offsetof(DMUI_ThemeColors, accentMuted) == 100);
static_assert(offsetof(DMUI_ThemeColors, statusDisable) == 116);
static_assert(offsetof(DMUI_ThemeColors, statusError) == 132);
static_assert(offsetof(DMUI_ThemeColors, statusWarning) == 148);
static_assert(offsetof(DMUI_ThemeColors, statusRestartNeeded) == 164);
static_assert(offsetof(DMUI_ThemeColors, statusCurrentHotkey) == 180);
static_assert(offsetof(DMUI_ThemeColors, statusSuccess) == 196);
static_assert(offsetof(DMUI_ThemeColors, statusInfo) == 212);
static_assert(DMUI_THEME_COLORS_0_1_SIZE == sizeof(DMUI_ThemeColors));
static_assert(offsetof(DMUI_SettingsRowOptions, structSize) == 0);
static_assert(offsetof(DMUI_SettingsRowOptions, resetVisible) == 4);
static_assert(offsetof(DMUI_SettingsRowOptions, resetEnabled) == 8);
static_assert(DMUI_SETTINGS_ROW_OPTIONS_0_1_SIZE ==
	sizeof(DMUI_SettingsRowOptions));
static_assert(offsetof(DMUI_SettingsRowBeginOptions, structSize) == 0);
static_assert(offsetof(DMUI_SettingsRowBeginOptions, layout) == 4);
static_assert(DMUI_SETTINGS_ROW_BEGIN_OPTIONS_0_1_SIZE ==
	sizeof(DMUI_SettingsRowBeginOptions));
static_assert(offsetof(DMUI_PageActivityInfo, structSize) == 0);
static_assert(offsetof(DMUI_PageActivityInfo, kind) == 4);
static_assert(offsetof(DMUI_PageActivityInfo, previousPage) == 8);
static_assert(offsetof(DMUI_PageActivityInfo, activePage) == 16);
static_assert(DMUI_PAGE_ACTIVITY_INFO_0_1_SIZE ==
	sizeof(DMUI_PageActivityInfo));
static_assert(offsetof(DMUI_PageActivityObserverDescriptor, structSize) == 0);
static_assert(offsetof(DMUI_PageActivityObserverDescriptor, callback) == 8);
static_assert(offsetof(DMUI_PageActivityObserverDescriptor, userData) == 16);
static_assert(DMUI_PAGE_ACTIVITY_OBSERVER_DESCRIPTOR_0_1_SIZE ==
	sizeof(DMUI_PageActivityObserverDescriptor));
static_assert(offsetof(DMUI_FrameObserverDescriptor, structSize) == 0);
static_assert(offsetof(DMUI_FrameObserverDescriptor, callback) == 8);
static_assert(offsetof(DMUI_FrameObserverDescriptor, userData) == 16);
static_assert(sizeof(DMUI_HostAPI) == 264);
static_assert(offsetof(DMUI_HostAPI, structSize) == 0);
static_assert(offsetof(DMUI_HostAPI, apiVersion) == 4);
static_assert(offsetof(DMUI_HostAPI, imguiFingerprint) == 8);
static_assert(offsetof(DMUI_HostAPI, registerClient) == 16);
static_assert(offsetof(DMUI_HostAPI, registerPage) == 24);
static_assert(offsetof(DMUI_HostAPI, queryState) == 32);
static_assert(offsetof(DMUI_HostAPI, requestFrame) == 40);
static_assert(offsetof(DMUI_HostAPI, releaseFrame) == 48);
static_assert(offsetof(DMUI_HostAPI, isMenuVisible) == 56);
static_assert(offsetof(DMUI_HostAPI, selectPage) == 64);
static_assert(offsetof(DMUI_HostAPI, attachSwapChain) == 72);
static_assert(offsetof(DMUI_HostAPI, registerAction) == 80);
static_assert(offsetof(DMUI_HostAPI, setStatus) == 88);
static_assert(offsetof(DMUI_HostAPI, getThemeColors) == 96);
static_assert(offsetof(DMUI_HostAPI, pushFont) == 104);
static_assert(offsetof(DMUI_HostAPI, popFont) == 112);
static_assert(offsetof(DMUI_HostAPI, drawSectionHeader) == 120);
static_assert(offsetof(DMUI_HostAPI, drawSearchInput) == 128);
static_assert(offsetof(DMUI_HostAPI, drawCollapsingSectionHeader) == 136);
static_assert(offsetof(DMUI_HostAPI, drawSettingsActionButton) == 144);
static_assert(offsetof(DMUI_HostAPI, settingsActionButtonWidth) == 152);
static_assert(offsetof(DMUI_HostAPI, settingsActionButtonExtent) == 160);
static_assert(offsetof(DMUI_HostAPI, registerFrameObserver) == 168);
static_assert(offsetof(DMUI_HostAPI, queryVideoMemory) == 176);
static_assert(offsetof(DMUI_HostAPI, drawBulletText) == 184);
static_assert(offsetof(DMUI_HostAPI, registerHotkeyAction) == 192);
static_assert(offsetof(DMUI_HostAPI, queryHotkeyBinding) == 200);
static_assert(offsetof(DMUI_HostAPI, unregisterHotkeyAction) == 208);
static_assert(offsetof(DMUI_HostAPI, beginSettingsTable) == 216);
static_assert(offsetof(DMUI_HostAPI, beginSettingsRow) == 224);
static_assert(offsetof(DMUI_HostAPI, endSettingsRow) == 232);
static_assert(offsetof(DMUI_HostAPI, endSettingsTable) == 240);
static_assert(offsetof(DMUI_HostAPI, beginSettingsRowEx) == 248);
static_assert(offsetof(DMUI_HostAPI, registerPageActivityObserver) == 256);
static_assert(DMUI_HOST_API_SELECT_PAGE_SIZE == 72);
static_assert(DMUI_HOST_API_ATTACH_SWAP_CHAIN_SIZE == 80);
static_assert(DMUI_HOST_API_REGISTER_ACTION_SIZE == 88);
static_assert(DMUI_HOST_API_SET_STATUS_SIZE == 96);
static_assert(DMUI_HOST_API_GET_THEME_COLORS_SIZE == 104);
static_assert(DMUI_HOST_API_PUSH_FONT_SIZE == 112);
static_assert(DMUI_HOST_API_POP_FONT_SIZE == 120);
static_assert(DMUI_HOST_API_DRAW_SECTION_HEADER_SIZE == 128);
static_assert(DMUI_HOST_API_DRAW_SEARCH_INPUT_SIZE == 136);
static_assert(DMUI_HOST_API_DRAW_COLLAPSING_SECTION_HEADER_SIZE == 144);
static_assert(DMUI_HOST_API_DRAW_SETTINGS_ACTION_BUTTON_SIZE == 152);
static_assert(DMUI_HOST_API_SETTINGS_ACTION_BUTTON_WIDTH_SIZE == 160);
static_assert(DMUI_HOST_API_SETTINGS_ACTION_BUTTON_EXTENT_SIZE == 168);
static_assert(DMUI_HOST_API_REGISTER_FRAME_OBSERVER_SIZE == 176);
static_assert(DMUI_HOST_API_QUERY_VIDEO_MEMORY_SIZE == 184);
static_assert(DMUI_HOST_API_DRAW_BULLET_TEXT_SIZE == 192);
static_assert(DMUI_HOST_API_REGISTER_HOTKEY_ACTION_SIZE == 200);
static_assert(DMUI_HOST_API_QUERY_HOTKEY_BINDING_SIZE == 208);
static_assert(DMUI_HOST_API_UNREGISTER_HOTKEY_ACTION_SIZE == 216);
static_assert(DMUI_HOST_API_BEGIN_SETTINGS_TABLE_SIZE == 224);
static_assert(DMUI_HOST_API_BEGIN_SETTINGS_ROW_SIZE == 232);
static_assert(DMUI_HOST_API_END_SETTINGS_ROW_SIZE == 240);
static_assert(DMUI_HOST_API_END_SETTINGS_TABLE_SIZE == 248);
static_assert(DMUI_HOST_API_BEGIN_SETTINGS_ROW_EX_SIZE == 256);
static_assert(DMUI_HOST_API_REGISTER_PAGE_ACTIVITY_OBSERVER_SIZE ==
	sizeof(DMUI_HostAPI));
#endif
