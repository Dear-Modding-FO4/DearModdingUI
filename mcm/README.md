# MCM compatibility module

`dmui-mcm` reads Mod Configuration Menu configurations and maps them onto the settings vocabulary in
`Client.h`. It is a static library consumed by `dmui-tests`, `dmui-preview`, and the separate
`DearModdingUI-MCM` compatibility plugin; the host links no MCM or JSON code. The library remains
independent of rendering, game, and F4SE headers.

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

`ResolveTextPresentation` leaves markup literal unless a text control declares `"html": true`.
Opted-in text expands break and paragraph boundaries, strips tags, and decodes the five common named
entities plus decimal and hexadecimal numeric entities. A control-level alignment is the default;
the last valid paragraph alignment overrides it for the resolved read-only control.
The mapper stores presentations on mapped rows. Each final binary calls `AttachTextRendering` from
the consumer adapter outside this pure module.

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

Page activity drives refreshes for the complete mapped dependency set, including non-emitted
`hiddenSwitcher` controls. Draw-time reads and `groupCondition` evaluation consult snapshots only.
A pure `TaskScheduler` boundary moves every Papyrus VM operation to the game thread before resolving
attached scripts, dispatching events, refreshing values, writing values, or executing actions.
`PapyrusDispatcher` separately owns static scalar calls, so mod-setting reads and writes are testable
with a fake without linking game headers.
A pending condition hides its dependent rows until the controller resolves and an all-pending page
shows a loading note. A permanently inoperable mod-setting toggle owns page-local state only when its
`groupControl` is referenced by a condition, restoring accordion interaction without inventing
persistent state. Other inoperable controllers and missing or failed dependencies fail open, keep
dependent content visible, and add a page compatibility diagnostic. No unresolved source state is
replaced with the configured default.

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

## Actions

Buttons map to `SettingsActionRow`, not `SettingDescriptor`, so they have no default value, value
binding, dirty state, or reset behavior. `ActionExecutor` is a separate pure interface because
operations do not belong to value storage. `BindActions` also applies actions declared on value
controls, substitutes their effective value for typed `{value}` arguments, contains executor
exceptions, and reports failures as page notes. Successful completions refresh the page's existing
mapped bindings.

The runtime executor queues work through F4SE and supports `CallFunction` and
`CallGlobalFunction`. It resolves attached scripts when a member action omits `scriptName`, validates
Papyrus parameter count and scalar types, and dispatches only after validation. `CallExternalFunction`
remains disabled with an explanation because its `root.f4se.plugins` target requires a Scaleform
movie that is not present during ordinary gameplay. Console-command and event actions are also
disabled rather than silently ignored.

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
loaded in the process, then discovers `Data\MCM\Config\*\config.json` and registers each valid
configuration as an independent DearModdingUI client. The module signal proves MCM's native provider
is loaded and avoids assumptions about mod-manager filesystem virtualization. Each client owns a composite with
global, mod-setting, and property backends. Global forms resolve through `TESDataHandler`;
mod-setting getters and setters dispatch through the `MCM` Papyrus natives; property reads resolve
through `GetPropertyValue` callbacks and probe attached scripts when `scriptName` is absent.
Callback completions enter a queue and the settings page's per-frame preparation only pumps that
queue. Generations reject a refresh completion that predates a write.

The page lifecycle observer refreshes the active page and emits zero-argument `OnMCMOpen` and
`OnMCMClose` events without false close/open pairs during same-client page changes. Accepted
declared mod-setting writes emit both `OnMCMSettingChange` and its mod-specific form with
`(modName, controlId)`; event dispatch is separate from value storage.

Settings declarations are applied before `SummarizeCompatibility`. The page exposes unsupported,
unknown-source, undeclared-setting, action, image, and inert-reason counts, while logs include both warnings and
errors. Unsupported images retain their metadata, warning, and counts but emit no descriptor;
load-bearing unsupported controls remain visible and disabled. The preview accepts
`DMUI_PREVIEW_MCM_INSTALLED=0` and `DMUI_PREVIEW_GAME_LOADED=0` to inspect the missing-MCM and
main-menu states without adding command-line surface.
