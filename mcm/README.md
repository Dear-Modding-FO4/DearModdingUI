# MCM compatibility module

`dmui-mcm` reads Mod Configuration Menu configurations and maps them onto the settings vocabulary in
`Client.h`. It is a static library consumed by `dmui-tests` and the separate `DearModdingUI-MCM`
compatibility plugin; the host links no MCM or JSON code.

`ParseConfig` and `LoadConfig` are `noexcept` and total. Input is third-party JSON, so every failure
is diagnosed and skipped rather than thrown or aborted on, and condition nesting is capped at
`kMaxConditionDepth`. `ResolveSourceType` classifies each value source once at parse time, and
`MappedPage::bindings` correlates emitted descriptor ids with their source, so callers never
re-derive ids or re-parse source strings.

## Writing values

| Family | Path | Raises `OnMCMSettingChange` |
|---|---|---|
| `kGlobal` | assign `TESGlobal::value` | no, the mod reads the global |
| `kProperty` | write the Papyrus property slot | no, the mod reads the property |
| `kModSetting` | `MCM.SetModSetting*` through the Papyrus VM | yes |

Measured in game:

- MCM registers eleven natives on the Papyrus script `MCM`, including `IsInstalled`,
  `GetVersionCode`, and `RefreshMenu`. Dispatching them works from gameplay, independent of MCM's
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

`DearModdingUI-MCM` discovers `Data\MCM\Config\*\config.json` at F4SE `kPostPostLoad` and registers
each valid configuration as an independent DearModdingUI client. The first runtime slice supports
only global values. Forms resolve through `TESDataHandler` at `kGameDataReady`; reads afterward use
the cached `TESGlobal` pointer. MCM presence is queried through the `MCM.IsInstalled` Papyrus native.
When it is absent, pages remain visible with disabled controls and an explanatory note.
