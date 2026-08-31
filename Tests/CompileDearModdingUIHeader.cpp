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
static_assert(std::is_standard_layout_v<DMUI_Vec2>);
static_assert(std::is_trivially_copyable_v<DMUI_Vec2>);
static_assert(std::is_standard_layout_v<DMUI_Vec4>);
static_assert(std::is_trivially_copyable_v<DMUI_Vec4>);
static_assert(std::is_standard_layout_v<DMUI_ThemeColors>);
static_assert(std::is_trivially_copyable_v<DMUI_ThemeColors>);
static_assert(std::is_standard_layout_v<DMUI_HostAPI>);
static_assert(std::is_trivially_copyable_v<DMUI_HostAPI>);
static_assert(sizeof(DMUI_StatusSeverity) == sizeof(uint32_t));
static_assert(sizeof(DMUI_FontRole) == sizeof(uint32_t));
static_assert(sizeof(DMUI_SettingsAction) == sizeof(uint32_t));
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

#if UINTPTR_MAX == UINT64_MAX
static_assert(sizeof(DMUI_ImGuiFingerprint) == 216);
static_assert(sizeof(DMUI_HostReadyInfo) == 40);
static_assert(sizeof(DMUI_ClientDescriptor) == 72);
static_assert(sizeof(DMUI_PageDescriptor) == 64);
static_assert(sizeof(DMUI_ActionDescriptor) == 64);
static_assert(sizeof(DMUI_HostStateInfo) == 28);
static_assert(sizeof(DMUI_Vec2) == 8);
static_assert(sizeof(DMUI_Vec4) == 16);
static_assert(sizeof(DMUI_ThemeColors) == 228);
static_assert(offsetof(DMUI_Vec2, x) == 0);
static_assert(offsetof(DMUI_Vec2, y) == 4);
static_assert(offsetof(DMUI_Vec4, x) == 0);
static_assert(offsetof(DMUI_Vec4, y) == 4);
static_assert(offsetof(DMUI_Vec4, z) == 8);
static_assert(offsetof(DMUI_Vec4, w) == 12);
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
static_assert(DMUI_THEME_COLORS_1_0_SIZE == sizeof(DMUI_ThemeColors));
static_assert(sizeof(DMUI_HostAPI) == 168);
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
static_assert(DMUI_HOST_API_SETTINGS_ACTION_BUTTON_EXTENT_SIZE == sizeof(DMUI_HostAPI));
#endif
