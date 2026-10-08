# MCM compatibility module

`dmui-mcm` maps Mod Configuration Menu configurations onto the settings vocabulary in `Client.h`.
It is a static library used by `dmui-tests`, `dmui-preview`, and the `DearModdingUI-MCM` plugin;
the host links no MCM or JSON code.

| Path | Owns |
|---|---|
| `include`, `src` | Pure parsing, mapping, value caches, conditions, and page binding; no game or F4SE headers |
| `adapters` | Consumer rendering through stable `dmui::ui` calls (`AttachTextRendering`) |
| `runtime` | The bridge plugin and game-facing adapters; the only F4SE dependency |

## Contracts

- **Total parsing.** `ParseConfig` and `LoadConfig` are `noexcept`: third-party JSON failures are
  diagnosed and skipped, never thrown.
- **Localization.** An optional resolver translates exact `$KEY` display text, including HTML text
  nodes. Identities, bindings, stored values, and action arguments are never translated. Missing keys
  stay visible with bounded diagnostics.
- **MCM fidelity.** Text markup, slider ranges and quantization, `dropdownFiles` enumeration, and
  keybind matching follow the shipped MCM widgets, not the core settings API's conventions.
- **Snapshots.** `ValueSource::Read` returns a generation-stamped ready, pending, missing, or failed
  snapshot and never dispatches. Async sources discard completions older than their generation.
  Unready rows draw the typed default, disabled.
- **Game thread.** Every Papyrus VM operation goes through the `TaskScheduler` boundary.
- **Conditions fail open.** Unresolvable dependencies keep content visible and add a page diagnostic;
  no unresolved state is replaced with a configured default.

## Writing values

| Family | Path | Raises `OnMCMSettingChange` |
|---|---|---|
| `kGlobal` | assign `TESGlobal::value` | no |
| `kProperty` | write the Papyrus property slot | no |
| `kModSetting` | `MCM.SetModSetting*` through the Papyrus VM | yes |

Buttons are `SettingsActionRow`s with no value, dirty state, or reset. The runtime executes
`CallFunction`, `CallGlobalFunction`, and `CallExternalFunction`; console-command and event actions
are disabled rather than ignored. Hotkeys are read-only, because MCM dispatches from its own
in-memory map.

## Runtime plugin

`DearModdingUI-MCM` detects MCM at `kPostPostLoad` by checking whether `mcm.dll` is loaded. At
`kGameLoaded` it registers each valid `Data\MCM\Config\*\config.json` as an independent client,
resolving `$KEY` text through the host's `resolveText`. Page activity refreshes values and emits
MCM's menu and setting-change events at mod boundaries.

Health receives parser diagnostics plus the actionable faults found afterward: unsupported
runtime actions and undeclared persisted settings. The preview accepts
`DMUI_PREVIEW_MCM_INSTALLED=0` and `DMUI_PREVIEW_GAME_LOADED=0` for the missing-MCM and main-menu
states.
