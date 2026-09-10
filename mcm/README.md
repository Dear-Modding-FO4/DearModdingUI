# MCM compatibility module

`dmui-mcm` reads Mod Configuration Menu configurations and maps them onto the settings vocabulary in
`Client.h`. It is a static library consumed by `dmui-tests`, `dmui-preview`, and the separate
`DearModdingUI-MCM` compatibility plugin; the host links no MCM or JSON code. The library remains
independent of rendering, game, and F4SE headers.

The `runtime` subdirectory contains the separate bridge plugin's entry point and
game-facing adapters. Only that target depends on F4SE. Pure parsing, control
decoding and mapping, value caches and conditions, value-source routing, and
page binding stay under `src\configuration`, `src\mapping`, and `src\bindings`,
with shared private helpers in `src\support`. Public pure interfaces stay under
`include`. Synthetic MCM scenarios live in `..\tools\preview\fixtures`, not in
production bridge code.

`ParseConfig` and `LoadConfig` are `noexcept` and total. Input is third-party JSON, so every failure
is diagnosed and skipped rather than thrown or aborted on, and condition nesting is capped at
`kMaxConditionDepth`. Invalid string escapes pass through as literal backslashes. Comments and
trailing commas remain invalid rather than carrying unused normalization policy.

`MappedPage::rows` keeps each control's binding, text presentation, action, and image metadata
together, including descriptorless presentational images. Global, property, and mod-setting bindings
are distinct variant alternatives. Property bindings retain the script name, while mod-setting
bindings retain normalized section and key names plus declared, undeclared, or unknown declaration
state.

## Text presentation

`ResolveTextPresentation` leaves markup literal unless a text control's `html` value is truthy under
AVM2 semantics: nonzero numbers, nonempty strings, arrays, and objects opt in as well as `true`;
zero, empty strings, `null`, and an absent field do not. This matches upstream
`SettingsOptionItem.as` and the installed MCM text-control bytecode. Opted-in text expands break and
paragraph boundaries, strips tags, and decodes the five common named entities plus decimal and
hexadecimal numeric entities. A control-level alignment is the default; the last valid paragraph
alignment overrides it for the resolved read-only control.
The mapper stores presentations on mapped rows. Each final binary calls `AttachTextRendering` from
the game-independent consumer adapter at `adapters\include\DearModdingUI\MCM\TextRendering.h`.
Consumers add `mcm\adapters\include` and include `<DearModdingUI/MCM/TextRendering.h>`.
There is no rendering adapter in the pure library's include root. Rendering goes
through stable `dmui::ui` calls rather than a raw ImGui dependency.
Section headings use the same opt-in markup normalization before category icon inference.

`ParseConfig` and `LoadConfig` accept an optional display-only resolver for exact `$...` keys,
including whole text nodes in HTML-enabled text. `LoadResult::displayName` carries the client label
independently of page labels. Headings, labels, help, read-only text, and choice labels are localized;
raw configuration, generated identities, bindings, stored values, and action arguments are not.
Missing keys stay visible with bounded, deduplicated diagnostics. Without a resolver, behavior is unchanged.

## Value snapshots

`ValueSource::Read` returns ready, pending, missing, or failed snapshot state and never dispatches.
Every state carries a generation. `Refresh` returns its request generation, and `Write` returns a
snapshot containing the effective stored value. Async sources must discard completions older than
their current generation. Non-ready or mismatched values bind to the descriptor's correctly typed
default for safe drawing and disable the row until a ready value exists.

`CompositeValueSource` routes each mapped row independently, so a page may mix source families.
`ParseSettingsIni`, `LoadSettingsIni`, and `ApplyDeclarations` provide the pure declaration path for
mod-setting bindings. An unavailable `settings.ini` leaves declarations unknown and therefore
attemptable; only settings proven absent are disabled.

`ParseKeybindDefinitions` and `LoadKeybindDefinitions` read each mod's
`Data\MCM\Config\<folder>\keybinds.json`; `ParseUserKeybinds` and `LoadUserKeybinds` read the global
`Data\MCM\Settings\Keybinds.json`. A hotkey matches by definition `modName` and control `id`.
The runtime reads the global user file once per configuration discovery pass
and applies that immutable snapshot to every discovered mod.
Declared keys without a user entry render unbound, while controls absent from the definitions file
render as unable to be bound. The keyboard, mouse, and gamepad names use F4SE's unified DirectInput
macro codes. Hotkeys are deliberately read-only: MCM dispatches from its in-memory map, so external
writes do not apply until a game load and its next save or menu-close keybind commit overwrites them.

Page activity drives refreshes for the complete mapped dependency set, including non-emitted
`hiddenSwitcher` controls. Draw-time reads and `groupCondition` evaluation consult snapshots only.
A pure `TaskScheduler` boundary moves every Papyrus VM operation to the game thread before resolving
attached scripts, dispatching events, refreshing values, writing values, or executing actions.
`PapyrusDispatcher` separately owns static scalar calls, so mod-setting reads and writes are testable
with a fake without linking game headers.
Every async runtime source also requires a `DiagnosticReporter`. The pure module reports only
exceptions that would otherwise drop a completion or leave queued state unapplied; parser and
binding failures continue to return durable `Diagnostic` or result values. The plugin implements
the reporter with REX logging, while tests and the preview retain reported diagnostics in memory.
A pending condition hides its dependent rows until the controller resolves and an all-pending page
shows a loading note. A permanently inoperable mod-setting toggle owns page-local state only when
its `groupControl` is referenced by a condition. An idless `ModSettingBool` switch uses the same
route when referenced. The bridge initializes it false, matching MCM, and handles its disclosure
state without persistent reads, writes, refreshes, resets, or setting-change events.
This restores transient disclosure controls without
inventing setting identifiers. Named declared or unknown settings remain source-backed. Other
inoperable controllers and missing or failed dependencies fail open, keep dependent content visible,
and add a page compatibility diagnostic. No unresolved source state is replaced with the configured
default.

Named MCM sections start collapsible groups. Unnamed sections inside a group preserve their source
position as divider rows; a leading unnamed section retains an implicit divider-headed group.

`BindPage` records each row's `ValueRoute` and owns the row's inert-state resolver.
`InertReasonMetadata` declares each reason as environment- or row-scoped alongside its text.
Condition state and local ownership resolve first; environment gates then govern enablement before
unsupported sources or controls, undeclared mod settings, and snapshot state. Environment reasons
surface once in the page note, while durable row reasons remain on their row. The governing reason
drives enablement and compatibility counts. Source-backed mod settings require MCM installation and
a loaded game; properties require a loaded game; globals and `kLocalUiState` remain operable at the
main menu.

## Writing values

| Family | Path | Raises `OnMCMSettingChange` |
|---|---|---|
| `kGlobal` | assign `TESGlobal::value` | no, the mod reads the global |
| `kProperty` | write the Papyrus property slot | no, the mod reads the property |
| `kModSetting` | `MCM.SetModSetting*` through the Papyrus VM | yes |

Global-backed choices use numeric option indexes encoded as descriptor strings. Reads format the
global as an integer index, and writes accept only numeric strings rather than option labels.

MCM sliders preserve the shipped widget's parameter semantics. When `max` is absent or null, the
widget does not consume any supplied `min` or `step`, so the bridge materializes its effective
`0..1` range and `0.05` step. Integer-backed sliders project that default to the observable stored
values `0` and `1`. When `max` is present, finite numeric `min`, `max`, and a positive `step` are
required; malformed controls remain visible but disabled with diagnostics. Slider quantization is
anchored at zero after clamping, matching MCM's scrollbar rather than the core settings API's
general minimum-origin convention. Declared defaults use the same normalization, so resets converge
on the effective value rather than repeatedly writing an unreachable off-grid default.
If an integer half-up snap would cross an `int64_t` boundary,
the nearest representable zero-origin grid point is used instead.

`dropdownFiles` remains a string-backed choice rather than a value-source special case. Its
`valueOptions.path` is passed verbatim to a Win32 `FindFirstFile`/`FindNextFile` adapter with the
optional `mask` defaulting to `*`. Enumeration is non-recursive, unsorted, includes every matched
entry (including directories and hidden entries), preserves filename case and extensions, and does
not canonicalize paths, so in-process mod-manager filesystem virtualization remains effective.
Missing directories and no matches produce only the `None` choice; other I/O failures disable the
row with a transient path-specific explanation and are retried on later page activations.

The cached choice list is refreshed when its page is activated, including after reopening the
overlay, and `prepareView` only applies a changed snapshot without performing I/O. The first option
has value `""` and label `None`; a real file named `None` remains a distinct option. Unknown stored
filenames display as `None` through the choice control's unmatched label while the binding retains
the exact stored value for ordinary modified-state and reset handling. User selection and actions
receive the exact bare filename with extension, while selecting or resetting to `None` persists an
empty string.

## Actions

Buttons map to `SettingsActionRow`, not `SettingDescriptor`, so they have no default value, value
binding, dirty state, or reset behavior. `ActionExecutor` is a separate pure interface because
operations do not belong to value storage. `BindActions` also applies actions declared on value
controls, substitutes their effective value for typed `{value}` arguments, contains executor
exceptions, and reports failures as page notes. Successful completions refresh the page's existing
mapped bindings.

The runtime executor queues work through F4SE and supports `CallFunction`,
`CallGlobalFunction`, and `CallExternalFunction`. It resolves attached scripts when a member action
omits `scriptName`, validates Papyrus parameter count and scalar types, and dispatches only after
validation. A pure `ScaleformInvoker` boundary lets the plugin invoke registered external functions
through a loaded HUD or pause-menu movie on the UI task queue. Missing movies, plugins, functions,
and rejected invocations remain distinct failures. Console-command and event actions are disabled
rather than silently ignored.

Papyrus scalar conversion is target-driven. Boolean targets accept booleans and numeric zero/nonzero;
integer targets accept Papyrus integers; floating targets accept integers or floats; string targets
accept strings or integer choice indexes. Hidden controls retain their declared source value type, so
integer property conditions no longer depend on boolean fallback conversion.

Measured in game:

- MCM registers eleven natives on the Papyrus script `MCM`, including `GetVersionCode` and
  `RefreshMenu`. Dispatching them works from gameplay, independent of MCM's
  menu, and MCM writes changes through to `MCM\Settings\<modName>.ini` itself.
- `root.mcm` is unreachable. It lives on `PauseMenu`, which is not instantiated until the player
  opens it.
- Writes reach only keys already in MCM's store. A `ModSetting*` key absent from the mod's
  `Config\<Folder>\settings.ini` silently discards writes, and the getter returns a default, so an
  absent key is indistinguishable from a `false` one. Read the declared key set from `settings.ini`
  or such a control renders as a toggle that will not move.

CommonLibF4 declares `BSScript::IStackCallbackFunctor::~IStackCallbackFunctor` without defining it;
deriving from it requires supplying one.

## Runtime plugin

`DearModdingUI-MCM` resolves installation at F4SE `kPostPostLoad` by checking whether `mcm.dll` is
loaded in the process. At `kGameLoaded`, after the game and F4SE have populated the Scaleform
translation table, it acquires the existing translator state and then discovers
`Data\MCM\Config\*\config.json` and registers each valid configuration as an independent
DearModdingUI client. The translator lookup uses exact keys and the active game language without
loading translation files separately. If the translator infrastructure is unavailable, registration
continues with raw keys and reports that infrastructure failure separately from missing entries.
The module signal proves MCM's native provider is loaded and avoids assumptions about mod-manager
filesystem virtualization. Each client owns a composite with
global, mod-setting, and property backends. Global forms resolve through `TESDataHandler`;
mod-setting getters and setters dispatch through the `MCM` Papyrus natives; property reads resolve
through `GetPropertyValue` callbacks and probe attached scripts when `scriptName` is absent.
Callback completions enter a queue and the settings page's per-frame preparation only pumps that
queue. Generations reject a refresh completion that predates a write.

The page lifecycle observer refreshes the active page and emits zero-argument `OnMCMMenuOpen`,
`OnMCMMenuOpen|<modName>`, and `OnMCMMenuClose|<modName>` events at mod boundaries.
`OnMCMOpen` and `OnMCMClose` instead bracket one visible overlay session, so mod-to-mod navigation
does not produce false whole-menu close/open pairs. Accepted declared mod-setting writes emit both
`OnMCMSettingChange` and its mod-specific form with `(modName, controlId)`; event dispatch is
separate from value storage.

Settings declarations and action availability are resolved before registration diagnostics are
classified. Static compatibility counts remain INFO-level debug logs. Durable Health warnings are
limited to actionable faults discovered after parsing: unsupported runtime actions and undeclared
persisted mod settings. Locally owned disclosure toggles are not persisted settings and therefore
do not produce that warning. Supported action counts, false or pending conditions, optional unbound
keys, pre-save runtime availability, and pending or unavailable value snapshots remain live page
state rather than append-only startup diagnostics.

Parser diagnostics are logged before a client is created, including terminal
failures that produce no configuration. A connected client then receives each
durable diagnostic once; transient runtime failures remain log-only.
Parser diagnostics continue to own unknown or unsupported controls, sources, and images, so their
original warning severity, source location, and message reach Health without a duplicate page
summary. Unsupported images retain their metadata, conditions, warning, and counts but emit no
descriptor. Each warning identifies the unrendered `library::class` SWF component, or names the
missing metadata without inventing an identity.
An image-only page explains that limitation in a page note rather than adding an empty-descriptor
warning. Genuinely empty pages and malformed controls retain their diagnostics.
Load-bearing unsupported controls remain visible and disabled. The preview accepts
`DMUI_PREVIEW_MCM_INSTALLED=0` and `DMUI_PREVIEW_GAME_LOADED=0` to inspect the missing-MCM and
main-menu states without adding command-line surface.
