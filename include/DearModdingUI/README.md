# DearModdingUI client API

`API.h` is the C ABI for Dear-Modding F4SE user interfaces, and `ImGuiFingerprint.h` is the optional
C++ fingerprint builder. They live with `Client.h` and the shared visual helpers in the standalone
DearModdingUI API repository, which CommonLibF4 re-exports through its public
`lib/dearmoddingui-api` dependency. Linking that fork is enough to consume them. `Client.h` is a
header-only C++ wrapper that handles discovery, registration, and either forwarding or the ImGui
context handoff; prefer it over the raw ABI. Forwarding clients compile no Dear ImGui sources, while
lockstep clients link the pinned sources. Neither mode links against the host DLL or exposes Addictol,
CommonLibF4, F4SE, Windows, D3D, TOML, or C++ library types through the C contract.

## Discovery and registration

At F4SE `kPostPostLoad`, after every plugin `Load` has returned, locate the host DLL and resolve the
single `DMUI_GetHostAPI` export. Call it with `DMUI_API_VERSION_CURRENT`. A null result means that ABI
version is unsupported. Discovery may succeed before the host plugin initializes; `queryState` and
registration then return `DMUI_RESULT_HOST_NOT_INITIALIZED`. Export presence does not mean the
renderer is ready: register at `kPostPostLoad` and wait for exactly one lifecycle callback.

Client, page, action, hotkey-action, frame-observer, and page-activity-observer registration closes when the first valid
active-swapchain `Present` begins host initialization. Register them immediately after the client. All descriptor strings are copied;
callback and userdata pointers must remain valid for the process lifetime. IDs use ASCII letters,
digits, `.`, `_`, and `-`. Client IDs are process-wide; page and action IDs are unique within their client.
Page categories and summaries are optional; null or empty category means the page has no group.
The optional client `iconName` is copied at registration. Any canonical Phosphor 2.1.2 icon name is
valid; hyphens, spaces, underscores, and PascalCase normalize to the same slug. An unknown or null
value falls back through category and whole-word display-name concepts, then the question glyph. Set only documented
`DMUI_ClientDescriptor::capabilities`; unknown bits reject the descriptor. Client origin defaults to
`DMUI_CLIENT_ORIGIN_NATIVE`. A bridge sets `DMUI_CLIENT_ORIGIN_BRIDGED` and may provide a copied
`bridgeSourceLabel`; the Home page groups bridged clients under `<source> mods`, or `Bridged mods`
when no source label is supplied. Native clients must not provide a bridge source label.

Settings pages draw only inside the common modal menu. Overlay pages draw without input capture while
their reference-counted frame demand is nonzero. Balance every successful `requestFrame` with
`releaseFrame`. Settings pages reject frame demand. The common toggle controls modal visibility and
game-input suppression; overlay demand never suppresses input.

## Shared menu

The Evil Modding window owns all navigation chrome. Its header shows the host and selected client as a
breadcrumb with the undocked close control. While host interface settings are open, the breadcrumb
names that view instead of the selected client. Its mod dropdown is built from registered client
display names. The sidebar places uncategorized settings pages first without a heading, then groups
categorized pages under their category headings. Pages order by category, `sortKey`, display name,
and ID. Switching mods selects that client's first page. Overlay pages never
appear there. `selectPage` accepts settings pages, switches both the active mod and page, opens the
window, and falls back deterministically if the previous selection is not available.
The command palette searches mods, pages, and actions globally. A matching mod ranks above its pages
and opens its lowest-`sortKey` landing page while expanding that mod in the sidebar.

Clients receive a clean scrolling content region below the host-owned page title, category, and
summary. Draw regular ImGui controls there. Do not begin independent top-level windows, draw over
the sidebar/header, change the host style or fonts directly, or retain pointers into host navigation
data. Client pages inherit the active theme and may use their own balanced child regions and popups.

The host ports Community Shaders' current default palette, style dimensions, Jost Body, Title,
Heading, Subheading, and Subtext roles, resolution scaling, search and navigation treatments,
rounded title-bar highlights, footer, docking, and background blur around the neutral registry.
Layout is saved to `Data\F4SE\Plugins\DearModdingUI\imgui.ini`. Fonts, icons, and blur shaders load
only from that neutral root. Explicit names select from the complete Phosphor Fill catalog before
semantic concepts are considered. Icons use the accent tint by default. The footer gear
toggles a host-only settings view inside the existing scrolling content pane without changing the
active client page or adding an entry to the mod dropdown. The view closes from its title-row control,
the gear, Escape, or a mod selection. It exposes an accent picker with color-vision-friendly presets,
colored or monochrome icon tint, host-window opacity, command-palette color and opacity, background
blur and safe per-frame strength, accessibility UI scale, and body-font family. It also reports
resolved typography size and effective UI scale as read-only facts. Appearance options preview from
a local draft; Apply persists all editable values once to
`Data\F4SE\Plugins\DearModdingUI.toml`, while Revert or leaving the view discards the draft. UI scale
and body-font changes rebuild the atlas only after Apply. Editable values use the `[Additional]` TOML
table.

Body-font families are enumerated from subfolders of
`Data\F4SE\Plugins\DearModdingUI\Fonts`; the selected regular face is rebuilt only between frames.
Atkinson Hyperlegible and Jost ship with the host, and users can add another family without changing
code. A missing or failed family falls back to Jost, while a missing icon font falls back to text-only
labels without disabling the menu or the C ABI host.
When a normalized category name equals its client's normalized display name or full client ID, the
category inherits that client's resolved glyph. Other category labels try a Phosphor name before the
semantic concept vocabulary.
The semantic concepts are `ai`, `armor`, `audio`, `building`, `camera`, `combat`,
`compatibility`, `controls`, `crafting`, `debug`, `dev-tools`, `diagnostics`, `dialogue`, `difficulty`,
`economy`, `gameplay`, `general`, `graphics`, `hud`, `input`, `interface`, `inventory`, `leveling`,
`lighting`, `logging`, `map`, `memory`, `misc`, `network`, `npc`, `other`, `overlay`, `performance`,
`perks`, `physics`, `post-process`, `power-armor`, `quest`, `radio`, `save`,
`settlement`, `skills`, `stability`, `stealth`, `survival`, `ui`, `unloaded`, `vats`, `video`,
`visuals`, `weapons`, and `weather`.

## Shared theme and widgets

The optional appended theme and widget entries expose the host's visual vocabulary without publishing
ImGui or C++ types in `API.h`. Gate every call with its matching
`DMUI_HOST_API_<ENTRY>_SIZE` constant and a non-null function pointer. These calls accept only a
registered client and are available while the host is ready on the render thread.

`getThemeColors` fills a caller-sized `DMUI_ThemeColors` with the current accent, muted accent, success,
warning, error, info, and muted colors plus every status color. `pushFont` accepts the Body, Title,
Heading, Subheading, or Subtext role; balance every successful push with `popFont`. The C++ wrapper
provides `dmui::FontGuard` and converts `DMUI_Vec4` to `ImVec4` with `dmui::ToImVec4`.

`drawSectionHeader`, `drawCollapsingSectionHeader`, `drawSearchInput`,
`drawSettingsActionButton`, `settingsActionButtonWidth`, and `settingsActionButtonExtent` are thin
calls into the same helpers used by the host. The sizing calls return live host font and style
measurements through `float` output parameters. Search buffers must have a nonzero capacity and
contain a NUL terminator within that capacity. A successful call always leaves the buffer
NUL-terminated, truncates edited output to `capacity - 1`, and reports whether the text changed
through the fixed-width output flag. The C++ wrapper marshals this contract to `std::string&` and
returns sizing results through `std::optional<float>`.

`beginSettingsTable`, `beginSettingsRow`, `beginSettingsRowEx`, `endSettingsRow`, and `endSettingsTable` form the
host-owned label/value geometry bracket for settings pages. Both begin calls report clipping through
their `visible` output: call the matching end only when `visible` is nonzero. Each row has a stable
caller-supplied ID, a label, and an optional description; the host copies the text, draws the label
column, opens the value cell, reserves the reset column from live font/style metrics, and draws Reset
through the shared settings-action treatment. `DMUI_SettingsRowOptions` controls reset visibility and
enabled state and must provide at least `DMUI_SETTINGS_ROW_OPTIONS_0_1_SIZE`.
Declarative descriptors may provide `resolveDescription` when the explanation depends on live state.
The appended `beginSettingsRowEx` accepts a caller-sized `DMUI_SettingsRowBeginOptions`. Its
`DMUI_SETTINGS_ROW_LAYOUT_FULL_SPAN` layout gives the row content both table columns at begin time;
`DMUI_SETTINGS_ROW_LAYOUT_LABEL_VALUE` preserves the original geometry. The C++ `RowPresentation`
keeps label visibility and row layout independent. It prefers the extended entry when available and
falls back to `beginSettingsRow`, so a full-span request remains usable in the older value column.
Clients migrating from the former `SettingDescriptor::labelMode` field should assign
`SettingDescriptor::presentation.labelMode` instead.

`SettingsActionRow` is the inline, non-setting counterpart to `SettingDescriptor`. Groups retain
settings and actions separately and use `SettingGroup::rows` when their source order must be
preserved. A `DividerRow` in that ordering draws the shared divider without contributing to the
group's visible-row count. Action rows share `RowPresentation`, visibility, enabled state,
filtering, descriptions, and the host settings-table geometry without acquiring defaults, bindings,
dirty state, or reset semantics. They use the forwarded ordinary ImGui button primitive;
`drawSettingsActionButton` is reserved for the fixed Reset, Revert, and Apply actions and cannot
represent arbitrary labels.

`NumericSettingControl<T>::quantization` carries both an interval and an origin. Accepted edits use
`origin + round((value - origin) / interval) * interval`, then the setting binding returns the
effective stored value. `dragSpeed` remains an interaction-speed setting and does not encode storage
quantization.

The bracket is valid only on the render thread during that client's page callback. Settings brackets
cannot nest or reenter, and a row cannot begin without an open settings table. Calls outside the active
settings-page callback return `DMUI_RESULT_WRONG_THREAD`. Balanced ordinary ImGui tables may surround
the bracket or appear inside a value cell. A mismatched call returns
`DMUI_RESULT_UNBALANCED_BRACKET` without guessing which client stack entry to close. At the callback
boundary, the existing ImGui recovery restores any abandoned table, row, or ID state before the host
clears its bracket state, so an early return cannot leak into shared chrome. The C++ wrapper exposes
the two begin calls as `std::optional<bool>`, constructs the versioned row options, and applies every
appended-table availability check.

The C++ wrapper accepts page metadata through `dmui::PageDescriptor`, where category and summary are
optional, and returns the accepted page handle from `AddPage` as `std::optional<DMUI_PageHandle>`.
Pass that handle to `SelectPage` to select the registered settings page and open the shared menu.
Both methods preserve `LastResult()` for failure details.

## Client actions

Clients may register actions through the optional appended `registerAction` entry. Check
`DMUI_HostAPI::structSize >= DMUI_HOST_API_REGISTER_ACTION_SIZE` and that the pointer is non-null before
using it. Actions belong to their client, appear on every one of that client's page-title rows, and
order by `sortKey` then stable ID. The host copies the ID, display label, optional Phosphor icon name,
and optional tooltip. A missing or unknown icon uses a compact text button without reserving unused
space for clients that register no actions.

Action callbacks run only when the host-rendered control is pressed. The host contains C++ exceptions
and Windows structured exceptions, recovers shared ImGui state, and permanently disables a faulting
action. Clients must not draw their own header, footer, or action chrome.

## Client hotkeys

The optional appended `registerHotkeyAction` entry registers a stable, process-wide namespaced action
ID, display name, suggested default chord, callback, and user data. The action ID must contain at least
two nonempty ASCII segments separated by `.`; each segment starts with a letter and continues with
letters, digits, `_`, or `-`. Registration rejects malformed IDs, duplicate IDs across all clients, and
unknown chord strings. Supported chords combine `Ctrl`, `Alt`, and `Shift` with the host key table, such
as `F11` or `Shift+F11`; `none` is an explicit unbound suggestion.

Registration success does not imply a binding. Query `queryHotkeyBinding` with the returned handle to
obtain the current canonical chord and a distinct state for bound, user-cleared, suggested-default
conflict, never set, saved-override conflict, or invalid saved override. The host persists user
overrides by stable action ID in the `[Hotkeys]` TOML table. Overrides for uninstalled clients remain
visible as not-registered rows in the host hotkey manager until the user removes them.

The appended `unregisterHotkeyAction` entry is render-thread-only and returns `WRONG_THREAD` otherwise.
Successful removal tombstones the action; queued events resolve dead and are discarded during dispatch.
No later callback for the action runs after unregister returns.
It retains the saved override as a not-registered row, reapplies it on re-registration, and immediately
recomputes bindings so another action can use the chord.

The window procedure decides and swallows bound presses, repeats, and matching releases synchronously.
It only queues callback events. Both press and release callbacks are dispatched FIFO on the render
thread beside frame observers after a successful displayed `Present`, so client render state needs no
cross-thread synchronization for hotkeys. Repeats are coalesced, events survive stalled presentation,
and the 512-event queue reserves release capacity for every accepted press. Overflow drops and logs a
whole press/release pair rather than leaving a client in a held state. The C++ wrapper exposes
`AddHotkeyAction`, `QueryHotkeyBinding`, and `UnregisterHotkeyAction` with appended-table guards.

## Frame observation and video memory

The optional `registerFrameObserver` entry accepts a descriptor with a callback and user data. The host
calls each observer on the render thread after every successful non-test active-swapchain `Present`,
regardless of menu visibility. Registration is permanent for the process lifetime. The host
contains C++ and Windows structured exceptions, recovers shared ImGui state, and permanently disables a
faulting observer. The C++ wrapper stores capturing callables in stable storage and returns the observer
handle from `AddFrameObserver`.

The appended `registerPageActivityObserver` entry reports client-scoped settings-page activity.
Entering the first page owned by a client produces `DMUI_PAGE_ACTIVITY_ACTIVATED`, switching between
that client's pages produces `DMUI_PAGE_ACTIVITY_CHANGED`, and leaving the client or closing the menu
produces `DMUI_PAGE_ACTIVITY_DEACTIVATED`. The event carries previous and active page handles, using
the invalid page handle only across a client boundary. This lets a client implement one menu-open and
one menu-close notification without inferring deselection from missing draw calls. Callbacks run on
the render thread inside the shell draw. Registration lasts for the process lifetime and
has no unregister counterpart. The C++ wrapper stores callbacks in stable storage and exposes
`AddPageActivityObserver`.

The optional `queryVideoMemory` entry returns current local-segment usage and budget in bytes from the
adapter retained from the active swapchain. A non-OK result means no authoritative information is
available. The C++ wrapper returns `std::optional<dmui::VideoMemoryInfo>`. Gate both entries with their
published size constants and non-null function pointers when using the C ABI directly.

## Shared status

Clients may report status through the optional appended `setStatus` entry. Check
`DMUI_HostAPI::structSize >= DMUI_HOST_API_SET_STATUS_SIZE` and that the pointer is non-null before
using it. Pass the accepted client handle, one of `DMUI_STATUS_SEVERITY_INFO`,
`DMUI_STATUS_SEVERITY_SUCCESS`, `DMUI_STATUS_SEVERITY_WARNING`, or
`DMUI_STATUS_SEVERITY_ERROR`, and a non-empty null-terminated UTF-8 message. The host copies the
message before `setStatus` returns; the client retains ownership and may release or reuse its buffer
afterward. An unaccepted handle returns `DMUI_RESULT_CLIENT_NOT_FOUND`; a null or empty message and an
unknown severity return `DMUI_RESULT_INVALID_ARGUMENT`. `setStatus` may be called from any thread and
never calls ImGui from the calling thread.

The footer reserves a fixed status area whether or not a message is active. It shows the most recent
host or client message globally, attributed to the host name or registered client display name, even
when another client's page is selected. Info and success messages expire after four seconds. Warning
and error messages persist until a newer message supersedes them or the user dismisses them. Long
messages are truncated with an ellipsis, and hovering shows the full attributed text.

```cpp
if (api->structSize >= DMUI_HOST_API_SET_STATUS_SIZE && api->setStatus)
{
	api->setStatus(
		clientHandle,
		DMUI_STATUS_SEVERITY_SUCCESS,
		"Settings saved.");
}
```

The modal host opens a registered, hidden Fallout 4 carrier menu so absolute client coordinates remain
valid, then maps them into the attached backbuffer. The carrier movie and operating-system cursor stay
hidden while ImGui draws the only visible pointer. Closing the modal host releases Win32 cursor
ownership and removes the carrier from the menu stack. Overlay-only frames do not draw a cursor,
capture input, or open the carrier.

The standalone host initializes on the first valid active-swapchain `Present` whenever any client was
accepted. Clients can open the common menu by selecting one of their registered settings pages through
the host API. The existing host menu toggle remains in `[Additional]` for compatibility and reserves its
virtual key against client chords.

## Final swapchain handoff

A client that replaces the renderer's swapchain declares
`DMUI_CLIENT_CAPABILITY_RENDERER_REPLACEMENT` when registering. After it publishes the final native
swapchain, it may call the optional `attachSwapChain(clientHandle, nativeSwapChain)` entry. Check
`DMUI_HostAPI::structSize >= DMUI_HOST_API_ATTACH_SWAP_CHAIN_SIZE` and that the pointer is non-null
before calling it. On Windows/D3D11, `nativeSwapChain` is an `IDXGISwapChain*`; the public ABI keeps it
opaque and exposes no D3D types.

The host validates the client handle and capability, validates the swapchain's D3D11 device, immediate
context, and output window, installs its final `Present`/`ResizeBuffers` dispatch, and retains its own
COM references. Attachment is allowed while waiting for the first `Present` and after the host is
ready. A ready retarget keeps the shared ImGui context and safely reinitializes the platform/renderer
backends only if the render binding changed. An attachment racing backend initialization returns
`DMUI_RESULT_RENDERER_BUSY`; an invalid or unhookable native object returns
`DMUI_RESULT_SWAPCHAIN_REJECTED`. Regular clients receive
`DMUI_RESULT_CLIENT_CAPABILITY_REQUIRED`.

Discovery ignores unrelated swapchains while an attachment is active. Destruction of the active
window or a definitive DXGI device loss retires the attachment, releases host-owned COM/resources,
and permits the next discovered or explicit final swapchain to attach.

## ImGui compatibility and callbacks

The host publishes the immutable upstream commit, `IMGUI_VERSION_NUM`, explicit compile-configuration
flags, size and alignment fields for shared public/internal types, `ImDrawVert` member offsets, and a
deterministic layout signature. The signature is built from `sizeof`, `alignof`, and `offsetof`
expressions over public draw, font, IO, style, platform, context, and recovery structures. It is never
a copied magic value. A lockstep client must build its expected fingerprint from the exact headers and configuration used
to compile its own ImGui sources. Registration rejects any field mismatch before storing callbacks. A
null fingerprint selects layout-independent forwarding and skips the shared-layout comparison.

Include the pinned `imgui.h` and `imgui_internal.h`, then `ImGuiFingerprint.h`, and call
`DMUI_MakeImGuiFingerprint()`. The builder derives custom `ImTextureID`, `ImDrawIdx`, callback,
`ImDrawVert`, `ImWchar`, color packing, docking, obsolete API, test-engine, CRC, FreeType, debug-tool,
math-operator, and vector-extension flags directly from the active preprocessor configuration.

`onHostReady`, `onHostUnavailable`, page draw, action, hotkey, and frame callbacks run on the render thread.
`setStatus` is the exception and may be called from any thread. The context and allocator functions
exist only in `DMUI_HostReadyInfo`; clients must not poll for a context. In the ready callback, set the
client's statically linked ImGui globals:

```cpp
void DMUI_CALL Ready(const DMUI_HostReadyInfo* info, void*)
{
	ImGui::SetCurrentContext(static_cast<ImGuiContext*>(info->imguiContext));
	ImGui::SetAllocatorFunctions(
		info->imguiAlloc, info->imguiFree, info->imguiAllocatorUserData);
}
```

Client callback typedefs are intentionally not `noexcept`, so a C++ exception reaches the host guard
instead of terminating the process. Host API entry points and allocator callbacks remain `noexcept`.
The host catches C++ exceptions and Windows structured exceptions around client callbacks, disables a
faulting page or action, recovers the pinned ImGui stack state, and keeps the rest of the host usable.
Shared-context drawing cannot provide process isolation, so callbacks must still balance every ImGui
stack operation. The settings-table bracket additionally recovers abandoned bracket state at the
callback boundary; structural misuse still returns `DMUI_RESULT_UNBALANCED_BRACKET`.

If initialization fails, each accepted client receives `onHostUnavailable` with an explicit reason
and may start its standalone fallback. A client that receives `onHostReady` must stay hosted for the
process lifetime; hotkey actions may be unregistered, but clients cannot unload or hot reload.

## Minimal registration

```cpp
// Include imgui.h and imgui_internal.h before the fingerprint builder.
const auto fingerprint = DMUI_MakeImGuiFingerprint();

const auto getHost = reinterpret_cast<decltype(&DMUI_GetHostAPI)>(
	GetProcAddress(hostModule, "DMUI_GetHostAPI"));
const auto* api = getHost ? getHost(DMUI_API_VERSION_CURRENT) : nullptr;
if (!api)
{
	StartStandalone();
	return;
}

DMUI_ClientDescriptor client{
	sizeof(client),
	DMUI_API_VERSION_CURRENT,
	"example.author.mod",
	"Example Mod",
	DMUI_MAKE_VERSION(1, 0),
	&fingerprint,
	&Ready,
	&Unavailable,
	nullptr,
	DMUI_CLIENT_CAPABILITY_NONE,
	"puzzle-piece",
	DMUI_CLIENT_ORIGIN_NATIVE,
	nullptr
};
DMUI_ClientHandle clientHandle{};
if (api->registerClient(&client, &clientHandle) != DMUI_RESULT_OK)
{
	StartStandalone();
	return;
}

DMUI_PageDescriptor page{
	sizeof(page),
	"settings",
	"Settings",
	"General",
	"Example settings.",
	0,
	DMUI_PAGE_KIND_SETTINGS,
	&DrawSettings,
	nullptr
};
DMUI_PageHandle pageHandle{};
if (api->registerPage(clientHandle, &page, &pageHandle) != DMUI_RESULT_OK)
{
	StartStandalone();
	return;
}

if (api->structSize >= DMUI_HOST_API_REGISTER_ACTION_SIZE && api->registerAction)
{
	DMUI_ActionDescriptor action{
		sizeof(action),
		"refresh",
		"Refresh",
		"arrow-counter-clockwise",
		"Refresh this mod's data.",
		0,
		&Refresh,
		nullptr
	};
	DMUI_ActionHandle actionHandle{};
	if (api->registerAction(clientHandle, &action, &actionHandle) != DMUI_RESULT_OK)
		ReportActionRegistrationFailure();
}
```

A renderer-replacing client sets `client.capabilities` to
`DMUI_CLIENT_CAPABILITY_RENDERER_REPLACEMENT`, stores the returned API table and client handle, then
hands off its final published proxy:

```cpp
if (api->structSize < DMUI_HOST_API_ATTACH_SWAP_CHAIN_SIZE ||
	!api->attachSwapChain ||
	api->attachSwapChain(clientHandle, finalSwapChain) != DMUI_RESULT_OK)
{
	ReportHandoffFailure();
}
```
