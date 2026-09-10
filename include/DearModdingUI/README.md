# DearModdingUI client API

`API.h` is the host C ABI for Dear-Modding F4SE user interfaces. It lives with
`CUIAPI.h`, `UI.h`, `Client.h`, and the shared visual helpers in the standalone
DearModdingUI API repository, which CommonLibF4 re-exports through its public
`lib/dearmoddingui-api` dependency. Linking that fork is enough to consume
them. `Client.h` is a header-only C++ wrapper that handles discovery,
registration, stable UI negotiation, and callback error propagation; prefer it
over the raw ABI. Clients compile no Dear ImGui sources and do not link against
the host DLL. Client drawing uses the separate `dmui::ui` namespace and
DMUI-owned types. The C contract exposes no Addictol, CommonLibF4, F4SE,
Windows, D3D, TOML, Dear ImGui, or C++ library types.

## Discovery and registration

At F4SE `kPostPostLoad`, after every plugin `Load` has returned, locate the host DLL and resolve the
single `DMUI_GetAPI` export. Call it with `DMUI_HOST_ABI_CURRENT`, then validate the returned
`hostAbiVersion`. A null result means that host ABI generation is unsupported.
The product/API release identifier remains `DMUI_API_VERSION_CURRENT`; the
returned `apiVersion` and client descriptor `apiVersion` are metadata, not
compatibility gates. Discovery may succeed before the host plugin initializes; `queryState` and
registration then return `DMUI_RESULT_HOST_NOT_INITIALIZED`. Export presence does not mean the
renderer is ready: register at `kPostPostLoad` and wait for exactly one lifecycle callback.

Host updates preserve unchanged operations at their existing table offsets and
append new operations. Clients negotiate optional capabilities independently, so
a missing new entry does not disable older operations they already use. Internal
implementation and file-layout changes do not change either ABI.

Client, category, page, action, hotkey-action, frame-observer, and page-activity-observer registration closes when the first valid
active-swapchain `Present` begins host initialization. Register them immediately after the client. All descriptor strings are copied;
callback and userdata pointers must remain valid for the process lifetime. IDs use ASCII letters,
digits, `.`, `_`, and `-`. Client IDs are process-wide; page and action IDs are unique within their client.
Categories are client-scoped first-class declarations with stable IDs, display names, and `sortKey`.
Register each category exactly once with `registerCategory`/`Client::AddCategory` before any page
references its `categoryId`. Unknown or cross-client category references fail; null or empty
`categoryId` leaves the page ungrouped. Categories are not synthesized from display strings, and
declared categories with no settings pages produce no empty heading.
The optional client, category, and page `iconName` values are copied at
registration. Any canonical Phosphor 2.1.2 icon name is valid; hyphens, spaces,
underscores, and PascalCase normalize to the same slug. An unknown, blank, or
null well-formed value falls through; malformed control text or a name over
128 bytes rejects the descriptor. Set only documented
`DMUI_ClientDescriptor::capabilities`; unknown bits reject the descriptor. Client origin defaults to
`DMUI_CLIENT_ORIGIN_NATIVE`. A bridge sets `DMUI_CLIENT_ORIGIN_BRIDGED` and may provide a copied
`bridgeSourceLabel`; the Health page groups bridged clients under `<source> mods`, or `Bridged mods`
when no source label is supplied. Native clients must not provide a bridge source label.

Settings pages draw only inside the common modal menu. Overlay pages draw without input capture while
their reference-counted frame demand is nonzero. Balance every successful `requestFrame` with
`releaseFrame`. Settings pages reject frame demand. The common toggle controls modal visibility and
game-input suppression; overlay demand never suppresses input.

## Shared menu

The Evil Modding window owns all navigation chrome. Its header shows the host and selected client as a
breadcrumb with the undocked close control. Home, Health, and Settings are host-owned sidebar pages
and name themselves in that breadcrumb. Home shows host identity, registration counts, and one overall
health summary derived from live subsystem and client status. Health owns the detailed host subsystem
observations and full client registry. Its mod dropdown is built from registered client display names.
The sidebar places uncategorized client settings pages first without a heading, then groups
categorized pages under their category headings. Categories order by `sortKey`, display name, and
stable ID; all-zero keys therefore sort alphabetically. Pages within a category order independently
by page `sortKey`, display name, and ID. Switching mods selects that client's first page. Overlay pages never
appear there. `selectPage` accepts settings pages, switches both the active mod and page, opens the
window, and falls back deterministically if the previous selection is not available.
The command palette searches mods, pages, and actions globally. A matching mod ranks above its pages
and opens its lowest-`sortKey` landing page while expanding that mod in the sidebar.
Navigation and normalized search metadata are built when registration closes;
search results reference that immutable data rather than rebuilding it each frame.
Named icon precedence is explicit valid name, catalog-driven metadata
inference, then the surface fallback. Clients infer from their display name
first and category display names second before Question. Category headings use
only their own explicit icon and display name before Question; they do not
inherit a matching client's icon. Palette pages infer from the page name and
category before Files. Actions infer once from their label and share that
selection between the toolbar and palette; an unmatched toolbar remains
text-only while an unmatched palette entry uses Terminal Window. Page icons
are palette-only: plain page sidebar and title rows retain their current text
and selection markers.
Raw-glyph links and section headers keep their separate contract: zero means
no icon, and an unavailable or unrepresentable glyph uses text fallback.
`SettingGroup::glyph` uses a chosen nonzero glyph or label inference;
`HeadingMode::kDivider` remains explicitly iconless. No icon editor, mod-name
exception table, category renaming, API version bump, or product version bump
is introduced.

Clients receive a clean scrolling content region below the host-owned page title, category, and
summary. Draw regular `dmui::ui` controls there. Do not begin independent top-level windows, draw over
the sidebar/header, change the host style or fonts directly, or retain pointers into host navigation
data. Client pages inherit the active theme and may use their own balanced child regions and popups.

The host ports Community Shaders' current default palette, style dimensions, Jost Body, Title,
Heading, Subheading, and Subtext roles, resolution scaling, search and navigation treatments,
rounded title-bar highlights, footer, docking, and background blur around the neutral registry.
Layout is saved to `Data\F4SE\Plugins\DearModdingUI\imgui.ini`. Fonts, icons, and blur shaders load
only from that neutral root. Explicit names select from the complete Phosphor Fill catalog before
semantic concepts are considered. Icons use the accent tint by default. The footer gear
navigates to the same host-owned Settings page exposed beside Home and Health in the sidebar; there is no
second dismissible settings panel. Navigating away or closing the menu discards unapplied previews,
matching the former panel dismissal behavior. The page exposes an accent picker with
color-vision-friendly presets, colored or monochrome icon tint, host-window opacity,
command-palette color and opacity, background
blur and safe per-frame strength, accessibility UI scale, and body-font family. It also reports
resolved typography size and effective UI scale as read-only facts. Appearance options preview from
a local draft; Apply persists all editable values once to
`Data\F4SE\Plugins\DearModdingUI.toml`, while Revert or leaving the view discards the draft. UI scale
and body-font changes rebuild the atlas only after Apply. Editable values use the `[Additional]` TOML
table.

The runtime retains normalized, typed settings rather than reparsing their TOML
representation during drawing. Save operations serialize those values, and a
failed save leaves both the active settings and unapplied draft unchanged.

Body-font families are enumerated from subfolders of
`Data\F4SE\Plugins\DearModdingUI\Fonts`; the selected regular face is rebuilt only between frames.
Atkinson Hyperlegible and Jost ship with the host, and users can add another family without changing
code. A missing or failed family falls back to Jost, while a missing icon font falls back to text-only
labels without disabling the menu or the C ABI host.
Category icon inference uses the human-readable category display name. Stable
category IDs are used for expansion identity, so equal labels with different
IDs remain distinct. The resolver indexes the complete shipped Phosphor names,
accepted upstream aliases, descriptive tags, and a small reviewed domain
vocabulary. It uses whole normalized words, reports ambiguity instead of
choosing alphabetically, and lets secondary metadata narrow only candidates
already found in primary metadata.

Icon inference is a header-only API helper rather than a new C ABI operation.
Older client binaries therefore retain the helper behavior they compiled until
they are rebuilt against the updated API. Raw-glyph section and link calls
still treat zero as no icon. An unset declarative `SettingGroup::glyph`
continues to request automatic label inference, and dividers remain iconless.

## Shared theme and widgets

The optional appended theme and widget entries expose the host's visual vocabulary without publishing
ImGui or C++ types in `API.h`. Gate every call with its matching
`DMUI_HOST_API_<ENTRY>_SIZE` constant and a non-null function pointer. These calls accept only a
registered client and are available while the host is ready on the render thread.

`getThemeColors` fills a caller-sized `DMUI_ThemeColors` with the current accent, muted accent, success,
warning, error, info, and muted colors plus every status color. `pushFont` accepts the Body, Title,
Heading, Subheading, or Subtext role; balance every successful push with `popFont`. The C++ wrapper
provides `dmui::FontGuard`, `dmui::DrawStyledText`, `dmui::DrawLabeledValue`, and converts `DMUI_Vec4` to
`dmui::ui::Vec4` with `dmui::ToUIVec4`. `TextStyle` independently selects an optional font role, semantic
`TextTone`, and wrapping. Semantic tones map directly to this theme snapshot; restart-needed and the
other status tones remain distinct from general warning/error colors, and muted text does not enter
ImGui's disabled-widget state. Drawing is length-delimited and unformatted, so long strings, `%`, and
`##` remain literal. `DrawLabeledValue` resolves theme and live spacing before drawing, acquires its
optional value font once, and preserves the caller's current font for the label. The existing
`DMUI_StyleMetrics` snapshot includes `fontSizeBase` after `scrollbarSize`, so clients use
one metrics path without a separate font-size export.

The public `SettingsTableScope` and `SettingsRowScope` own only successful visible begin calls and
preserve clipping as a successful invisible result. Their explicit ends are idempotent; row end
returns the optional Reset result. `DisabledScope` balances both enabled and disabled calls, while
`TooltipScope` owns rich tooltip content only after the requested hover test and a successful
`BeginTooltip`. The generic `DrawChoice` helper separates stable values and keys from visible labels,
supports disabled options, leaves unknown current values unchanged, and reports a completed change
only after selection of a different enabled option. Its optional final display-label argument is
drawn unformatted beside the combo and remains independent of the stable widget ID; omit it when a
settings row already owns the label geometry. See the API repository README for complete C++ signatures
and examples.

Declarative `ChoiceSettingControl::unmatchedLabel` customizes the unknown-value preview and defaults
to `"Unavailable"`. It never replaces the bound value: consumers can display `"None"` for a missing
selection while retaining ordinary change detection, reset behavior, and string persistence.

`drawSectionHeader`, `drawCollapsingSectionHeader`, `drawLinkRow`, `drawFaq`, `drawSearchInput`,
`drawSettingsActionButton`, `settingsActionButtonWidth`, and `settingsActionButtonExtent` are thin
calls into the same helpers used by the host. Link rows evenly divide the available width and perform
their explicitly selected Copy or Open action after a click. Existing host Home links remain copy-only;
disabled links remain hoverable so their note or target can explain the state. FAQ rows use host-owned disclosure state keyed by the widget ID
and entry index. The sizing calls return live host font and style measurements through `float` output
parameters. Search buffers must have a nonzero capacity and contain a NUL terminator within that
capacity. A successful call always leaves the buffer NUL-terminated, truncates edited output to
`capacity - 1`, and reports whether the text changed through the fixed-width output flag. The C++
wrapper marshals this contract to `std::string&` and returns sizing results through
`std::optional<float>`.

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
dirty state, or reset semantics. They use the stable `dmui::ui::Button` primitive;
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

The C++ wrapper accepts category metadata through `dmui::CategoryDescriptor` and
`Client::AddCategory`. Page metadata uses `dmui::PageDescriptor`, where `categoryId` and summary are
optional, and `AddPage` returns the accepted page handle as `std::optional<DMUI_PageHandle>`.
Pass that handle to `SelectPage` to select the registered settings page and open the shared menu.
Both methods preserve `LastResult()` for failure details.
Both descriptors append an optional `iconName`. The raw C ABI keeps
`DMUI_CATEGORY_DESCRIPTOR_0_1_SIZE == 32` and
`DMUI_PAGE_DESCRIPTOR_0_1_SIZE == 64`; set `structSize` to
`DMUI_CATEGORY_DESCRIPTOR_ICON_SIZE` (40) or
`DMUI_PAGE_DESCRIPTOR_ICON_SIZE` (72) before supplying the appended pointer.
Older prefixes retain inferred defaults, and partial appended pointers are
never read. Require `DMUI_HOST_SERVICE_NAVIGATION_ICONS` when honoring these
fields is mandatory.

## External opening and links

The pre-release API exposes the `DMUI_HOST_SERVICE_EXTERNAL_OPEN` service and the generic `openExternal` entry.
Without an explicit application, a URI, absolute file, or absolute directory is passed to the
operating system's registered handler. With an application override, `application` must be an
absolute executable path. The process argv is the executable path, followed by the supplied argument
array, followed by `target` when `targetKind` is not `NONE`. Arguments are never concatenated through
`cmd.exe` or PowerShell, and no placeholder substitution occurs. `workingDirectory` is accepted only
with an explicit application and must be absolute. Success means process creation or shell dispatch
was accepted; it does not promise that an application window appeared. Failures return
`DMUI_RESULT_EXTERNAL_OPEN_FAILED` and may report the native Windows error through `nativeError`.

`dmui::Link` uses the same `dmui::ExternalOpen` descriptor and an explicit `LinkAction`. Existing
copy behavior remains available as `kCopyTarget`; clients opt into `kOpenExternal` per link. Disabled
links do not execute either action, and an open failure is returned instead of falling back to the
clipboard. The operation is synchronous only through launch acceptance and never waits for the
external process to exit.

`DMUI_HOST_SERVICE_VIRTUAL_FILE_TARGETS` adds two explicit target kinds to the
same descriptor. `VIRTUAL_FILE` resolves the existing file visible at `target`
and opens its physical backing file. `VIRTUAL_FILE_PARENT` resolves that file
first, then opens its physical containing folder; it never resolves a merged
virtual directory. Both require an absolute file path and retain the same
OS-default or explicit-application behavior. Ordinary file/directory targets,
application paths, working directories, and copy links are not implicitly
resolved by the host.

Resolution opens existing files read-only, maps without reading their contents,
and obtains the backing section name. Ordinary handle-name queries are not used:
current USVFS deliberately rewrites them to the virtual name. Local volume mount
paths and supported UNC names are translated for external use without scanning
mod directories or consulting manager-specific configuration. Readable,
nonempty loose files are supported; empty files, directories, archive interiors,
and unsupported backing namespaces fail explicitly. Missing files have no
resolvable backing location. The host neither creates files nor guesses a
future Overwrite destination.

`EXTERNAL_RESOLUTION_FAILED` and `EXTERNAL_RESOLUTION_UNSUPPORTED` are distinct
from `EXTERNAL_OPEN_FAILED`; `nativeError` reports the Windows error for the
failing stage. Resolution failure never dispatches an unresolved fallback.
Resolution is synchronous, so call only for an explicit action, not every frame.
The physical target may belong to an installed mod; safe user-override creation
is client policy. A launch reopens by path and is not atomic with resolution.
USVFS may still inject child applications; this service does not promise an
unvirtualized process.

```cpp
uint32_t nativeError{};
const bool opened = client.OpenExternal({
    .targetKind = DMUI_EXTERNAL_TARGET_VIRTUAL_FILE_PARENT,
    .target = absoluteSettingsPath.c_str()
}, &nativeError);
```

If `opened` is false, surface `client.LastResult()` and `nativeError` through
the client's diagnostics.

## Client actions

Clients may register actions through the optional appended `registerAction` entry. Check
`DMUI_HostAPI::structSize >= DMUI_HOST_API_REGISTER_ACTION_SIZE` and that the pointer is non-null before
using it. Actions belong to their client, appear on every one of that client's page-title rows, and
order by `sortKey` then stable ID. The host copies the ID, display label, optional Phosphor icon name,
and optional tooltip. A missing or unknown icon uses a compact text button without reserving unused
space for clients that register no actions.
This toolbar contract is intentionally distinct from command-palette action
inference.

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

The appended `unregisterHotkeyAction` entry is render-execution-only and returns `WRONG_THREAD`
otherwise. Authorization belongs to the serialized active-`Present` callback scope, not to the first
OS thread that initialized the backend; a later `Present` may run on another thread. Client guards and
direct service calls do not grant authorization to arbitrary workers.
Successful removal tombstones the action; queued events resolve dead and are discarded during dispatch.
No later callback for the action runs after unregister returns.
It retains the saved override as a not-registered row, reapplies it on re-registration, and immediately
recomputes bindings so another action can use the chord.

The window procedure decides and swallows bound presses, repeats, and matching releases synchronously.
It only queues callback events. Both press and release callbacks are dispatched FIFO in the serialized
post-`Present` observer scope, so they cannot overlap page, action, or frame-observer callbacks even
when the game migrates `Present` between OS threads. Repeats are coalesced, events survive stalled presentation,
and the 512-event queue reserves release capacity for every accepted press. Overflow drops and logs a
whole press/release pair rather than leaving a client in a held state. The C++ wrapper exposes
`AddHotkeyAction`, `QueryHotkeyBinding`, and `UnregisterHotkeyAction` with appended-table guards.

## Frame observation and video memory

The optional `registerFrameObserver` entry accepts a descriptor with a callback and user data. The host
calls each observer in a non-drawing execution scope after every successful non-test active-swapchain
`Present`, regardless of menu visibility. The active attachment is revalidated after `Present`, so a
retired or rebound swapchain does not dispatch stale observers. The scope permits render services such
as image import before any UI frame is demanded, while drawing services still require a page draw
callback. Registration is permanent for the process lifetime. The host
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

## Client diagnostics

Clients may retain actionable problems through the optional appended `reportDiagnostic` entry. Check
`DMUI_HostAPI::structSize >= DMUI_HOST_API_REPORT_DIAGNOSTIC_SIZE` and that the pointer is non-null
before using it. Reports accept the shared status severity values, an optional scope, a required
one-line summary, and optional detail. The host copies all strings before returning.

`reportDiagnostic` may be called from any thread. Matching client, severity, scope, and summary values
aggregate into one retained record with an occurrence count; the first detail is preserved. Retention
is bounded per client, and the Health page and copied diagnostics report disclose how many further
reports could not be retained.

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

The host validates the client handle and capability, combines the override with the engine renderer's
device, immediate context, and current window, installs its final `Present`/`ResizeBuffers` dispatch,
and retains its own COM references. Attachment is allowed while waiting for the first `Present` and
after the host is ready. A ready retarget keeps the shared ImGui context and safely reinitializes the
platform/renderer backends. Missing engine renderer data, initialization, window,
swapchain, device, or context returns `DMUI_RESULT_HOST_NOT_READY`. An attachment
racing backend initialization or a change to the captured renderer binding returns
`DMUI_RESULT_RENDERER_BUSY`. The client may retain its final swapchain and retry
either transient result later; the host does not queue a rejected call.
An invalid or unhookable native object returns
`DMUI_RESULT_SWAPCHAIN_REJECTED`. Regular clients receive
`DMUI_RESULT_CLIENT_CAPABILITY_REQUIRED`.

The host reconciles against the engine's current renderer state throughout the session. An explicit
override remains authoritative while its engine device, context, and window are current; a renderer
generation change replaces it. Destruction of the active window or a definitive DXGI device loss
retires the attachment, releases host-owned COM/resources, and requests immediate reconciliation.

## Stable UI and presentation services

`queryServices` reports semantic host-service flags. The appended
`queryUIAPI` entry separately negotiates DMUI UI ABI family 1, a minimum
revision, and an additive function-table prefix. This identity is independent
of the API release label, the product version declared in
[xmake.lua](../../xmake.lua), and the host's internal Dear ImGui version.
Clients set `requiredServices`, `minimumUIRevision`, and
`minimumUIAPISize` in `ClientOptions`; the wrapper validates both services and
all required UI operations before `registerClient`.

The appended API provides official frame-demand and swapchain wrappers,
contextual hotkey enablement, owner/generation-scoped D3D11 image resources,
host-owned generic CPU-pixel images, opt-in managed overlay windows, copied
latest-message notifications, annotated plots, and single-active
submission-aware dialogs. CPU producers require
`DMUI_HOST_SERVICE_PIXEL_IMAGES`; imported SRVs retain the distinct
`DMUI_HOST_SERVICE_IMAGE_RESOURCES` promise. Existing host-table prefixes and
offsets remain unchanged; `queryUIAPI` is appended at the 440-byte generation-1
table size. See the nested public API README for the stable UI schema, exact
image formats, row
extent, transactional update, logical overlay coordinate, notification
duration, dialog state, and per-call thread contracts.

Stable `InputTextMultiline` and `IsItemDeactivatedAfterEdit` operations are
declared by the checked-in UI schema. Declarative setting writes remain live through
`binding.set`; `SettingDescriptor::onEdit` independently reports changed and
completed state immediately after the widget. New image and plot draw calls
are accepted only on the render thread during the owning page callback.
Image import and CPU create/update require a ready backend and bound render
thread, including frame observers, but no active draw callback. CPU calls
synchronously consume decoded RGBA8 bytes and retain no caller pointer.
Updates replace the GPU resource transactionally on the same handle, so queued
draws keep old pixels and later draws use the replacement. Image queries
require no draw phase. Notifications and image release are any-thread. Queued
image COM references remain leased through the actual `RenderDrawData` call.

Packed HDR color previews can import `R11G11B10_FLOAT` SRVs directly. The host
retains the original view and performs no HDR tone mapping or color conversion;
display mapping remains the producer's responsibility.

Depth previews can import `R16_UNORM`, `R24_UNORM_X8_TYPELESS`, `R32_FLOAT`,
or `R32_FLOAT_X8X24_TYPELESS` SRVs directly. Every imported format must also
advertise `D3D11_FORMAT_SUPPORT_TEXTURE2D` and
`D3D11_FORMAT_SUPPORT_SHADER_SAMPLE` on the owning device. Integer and
stencil-only views are rejected. The host samples the original SRV without
copying, normalizing, or converting depth into another texture.

## Stable UI compatibility and callbacks

Public clients never receive the host's Dear ImGui context, allocators, font
pointers, enum values, or internal layouts. `DMUI_GetAPI(HOST_ABI_1)` exposes
the host table, whose appended `queryUIAPI` entry negotiates stable UI ABI
family 1. Compatibility depends on the requested revision, required table
prefix, and non-null required operations, not on the host's internal Dear
ImGui version.

`onHostReady`, `onHostUnavailable`, page draw, action, hotkey, and frame callbacks run on the render thread.
`setStatus`, `postNotification`, image release, hotkey enablement, and dialog
submission resolution are the any-thread exceptions. `DMUI_HostReadyInfo` contains only its
size-prefixed release metadata; the callback is a lifecycle notification, not a context handoff:

```cpp
void DMUI_CALL Ready(const DMUI_HostReadyInfo* info, void*)
{
	if (!info || info->structSize < sizeof(DMUI_HostReadyInfo))
		return;
	// Registered page callbacks may now use dmui::ui.
}
```

Client callback typedefs are intentionally not `noexcept`, so a C++ exception reaches the host guard
instead of terminating the process. Host API entry points remain `noexcept`.
The host catches C++ exceptions and Windows structured exceptions around client callbacks, disables a
faulting page or action, recovers its internal UI stack state, and keeps the rest of the host usable.
Stable UI drawing remains in-process and cannot provide process isolation.
Familiar `dmui::ui` wrappers record the first operation error and the C++
client page trampoline returns it at the callback boundary; explicit checked
wrappers return `DMUI_Result` directly. Scope-end operations continue
dispatching after a sticky error so balanced scopes unwind. The settings-table
bracket additionally recovers abandoned bracket state at the
callback boundary; structural misuse still returns `DMUI_RESULT_UNBALANCED_BRACKET`.

If initialization fails, each accepted client receives `onHostUnavailable` with an explicit reason
and may start its standalone fallback. A client that receives `onHostReady` must stay hosted for the
process lifetime; hotkey actions may be unregistered, but clients cannot unload or hot reload.

## Minimal registration

```cpp
const auto getAPI = reinterpret_cast<decltype(&DMUI_GetAPI)>(
	GetProcAddress(hostModule, "DMUI_GetAPI"));
const auto* api = getAPI ? getAPI(DMUI_HOST_ABI_CURRENT) : nullptr;
if (!api ||
	api->structSize < DMUI_HOST_API_REGISTER_CLIENT_SIZE ||
	api->hostAbiVersion != DMUI_HOST_ABI_CURRENT)
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
	&Ready,
	&Unavailable,
	nullptr,
	DMUI_CLIENT_CAPABILITY_NONE,
	"cloud-sun",
	DMUI_CLIENT_ORIGIN_NATIVE,
	nullptr
};
DMUI_ClientHandle clientHandle{};
if (api->registerClient(&client, &clientHandle) != DMUI_RESULT_OK)
{
	StartStandalone();
	return;
}

DMUI_CategoryDescriptor category{
	sizeof(category),
	"lighting",
	"Lighting",
	0,
	0,
	"sun-horizon"
};
if (api->registerCategory(clientHandle, &category) != DMUI_RESULT_OK)
{
	StartStandalone();
	return;
}

DMUI_PageDescriptor page{
	sizeof(page),
	"settings",
	"Settings",
	"lighting",
	"Example settings.",
	0,
	DMUI_PAGE_KIND_SETTINGS,
	&DrawSettings,
	nullptr,
	"sliders-horizontal"
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
