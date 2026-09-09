# DearModdingUI

DearModdingUI is a standalone F4SE plugin that hosts one shared Dear ImGui menu for Fallout 4 mods. Client plugins discover `DearModdingUI.dll` at runtime and register settings or overlay pages through a versioned C ABI, so clients do not link against the host binary.

The project is pre-release (0.x); API-breaking changes are expected as the interfaces are simplified and improved. The host identity and plugin metadata use the version declared in `xmake.lua`.

The host owns the ImGui context, D3D11 and Win32 backends, common shell, navigation, fonts, theme, cursor, background blur, menu toggle key, and host appearance settings. Window layout is stored in `Data/F4SE/Plugins/DearModdingUI/imgui.ini`; host settings are stored in `Data/F4SE/Plugins/DearModdingUI.toml`. The `tree`, `twopane`, or `drilldown` sidebar selected by `[Additional] sMenuSidebarLayout` saves immediately, while cosmetic changes remain previews until Apply.

The host-owned Home page is the landing page for each game launch and gives a concise host identity, registration counts, and overall live-health summary. The peer Health page owns detailed host subsystem observations and the full client registry with per-mod status. Settings is the third host page, and the footer gear navigates to that same authoritative settings surface. Closing and reopening the menu within that launch returns to the last selected host or client page; active-page selection is not persisted across launches.

## Host health

Health reports renderer readiness, configuration loading and persistence,
game-input interception, and typography from the corresponding host operations.
Ready means the subsystem is operating as requested; Degraded means it remains
usable with a fallback or unresolved problem; Failed means a required capability
is unavailable. Waiting and Progressing describe initialization rather than
automatically indicating failure. An expired initialization deadline is shown
as a warning without changing the subsystem's operational state.

Missing optional configuration uses defaults normally. Invalid configuration,
failed saves, missing fonts or icons, and failed input hooks retain their actual
reasons until an observed recovery. Home, Health, logs, and copied reports use the
same observations. The existing Reported problems section remains separate and
last. The standalone preview's `--health-scenario synthetic` option adds clearly
labeled simulated outcomes without damaging installed assets or game hooks.

## Navigation

The sidebar groups settings clients under Native and each declared bridge-source label. Source names come from client metadata, not host-specific bridge rules. Empty page categories have no heading; named categories remain visible even when their names match a page or client.

The internal navigation model owns source membership independently of presentation. Section presentations describe visible sections and source controls; sidebar layouts arrange those sections and own their browsing state. A shared controller handles page selection from clicks, search, and client requests, then asks the presentation and layout to reveal that selection. This also keeps explicit same-page requests distinct from browsing back to a client list.

New source presentations implement the presentation contract in `NavigationPresentation.h`; new layouts implement activation, selection-reveal, and drawing through the sidebar contract. Layouts consume read-only navigation data and return navigation requests rather than changing the active page themselves. The sidebar descriptor catalog owns persisted layout IDs and production/preview availability. Grouped sections are the normal presentation; Native/Bridged destinations and the icon rail remain standalone-preview options, not additional production settings.

## Client registration

The host ABI, stable UI compatibility, lifecycle, and registration examples are documented in [`include/DearModdingUI/README.md`](include/DearModdingUI/README.md). Public client headers live in the standalone DearModdingUI API repository and arrive through CommonLibF4's `lib/dearmoddingui-api` public dependency.

Clients locate the `DMUI_GetAPI` export at F4SE `kPostPostLoad`, request
`DMUI_HOST_ABI_CURRENT`, validate the returned host ABI generation, negotiate
the stable UI table, register the client and all pages, then wait for the
host-ready callback before drawing. The 0.1 API value is release metadata, not
an additional compatibility gate. Clients may open a registered settings page
through `selectPage`; the host opens and closes the shared menu with
`[Additional] sMenuToggleKey`, which defaults to End. Escape first cancels the
active edit or drag, then dismisses the topmost popup or dialog, and closes the
shared menu when neither remains.

Clients can preflight the stable UI table and additive presentation services before
registration. The host now provides contextual hotkeys, retained D3D11 image
handles, host-owned RGBA8 images created and transactionally updated from
decoded CPU pixels, opt-in passive managed overlays, latest-message
notifications, annotated plots, and submission-aware confirm/text dialogs
without exposing ImGui context, draw-list, texture-ID, or IO types through the
C ABI. Full ownership, thread, row-extent, format, update, expiry, placement,
and dialog-state contracts are documented in the nested public API README.

## Building

Requirements:

- Visual Studio 2022 with the v143 C++ toolset
- [xmake](https://xmake.io/)

```powershell
git clone <repository-url>
cd DearModdingUI
git submodule update --init
git -C Depends/commonlibf4 submodule update --init --recursive
$projectRoot = (Resolve-Path -LiteralPath '.').Path
xmake f -P "$projectRoot" -m release --test-release=n
xmake build -P "$projectRoot" -y
xmake package-release -P "$projectRoot"
```

The release binaries are written to `.Build/release/F4SE/Plugins/`.
`package-release` creates two independently installable archives:

| Archive | Contents |
| --- | --- |
| `DearModdingUI-0.1.0-release.zip` | Host DLL, configuration, fonts, and shaders |
| `DearModdingUI-MCM-0.1.0-release.zip` | Optional MCM bridge DLL; requires the host |

The corresponding staging folders are `.Build/packages/release/DearModdingUI/`
and `.Build/packages/release/DearModdingUI-MCM/`. Archives start directly at
`F4SE/Plugins/`, so they can be uploaded as mod-manager downloads without
repacking. They contain only runtime files: DLLs and the host's configuration,
fonts, and shaders. READMEs, licensing documents, provenance files, and PDBs are
not included. Repository documentation is unchanged by package assembly.

CommonLib automatic install mappings are removed from all DMUI plugin targets.
Configure, build, and package never copy to a game or mod-manager path.

For the diagnostic distribution, configure the same checkout with
`--test-release=y`, build, and run `package-release`. Its separate `.Build/test/`
output produces `DearModdingUI-0.1.0-test.zip` with the host and
`dmui-test-client.dll`, and `DearModdingUI-MCM-0.1.0-test.zip` with the real MCM
bridge and its manually invoked Scaleform probe. The standard archives exclude
both diagnostics. Install both test archives for the complete diagnostic setup;
the core test package can also be used without the MCM bridge.
The standalone preview and test client use the same `DMUI Tests` page and state
implementation, including editable synthetic configuration and image, overlay,
dialog, notification, and input exercises. Game-only input-context observations
remain explicitly unexercised in the preview.

For each component, enable its test package instead of its release package, not
alongside it: the variants supply the same DLL name. Disable the older standalone
smoke-test mod when switching to this setup. Preserve your existing
`DearModdingUI.toml` and window layout rather than replacing them with packaged
defaults. Switch back to the release package after testing; fixture source stays
in the repository.

## Stable releases

Normal pushes and pull requests validate both variants and upload four separate
Actions artifacts: host and MCM, each in release and test form. They do not
publish GitHub releases.

Use **Actions > Build and release > Run workflow** from `main` and enter
`release_version` matching `plugin_version` in `xmake.lua`. Leave that input empty
to build without publishing. After the build succeeds, the workflow uploads the
two standard ZIPs and publishes a stable `v<version>` release at the exact run
commit. It creates no prereleases and makes no automatic version changes.
Existing releases are not overwritten. If an upload fails, inspect the retained
draft before retrying.

## Standalone preview

The `dmui-preview` target runs the production menu renderer in its own Win32/D3D11 window with representative fake clients:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
xmake build -P "$projectRoot" -y dmui-preview
.\.Build\Preview\dmui-preview.exe
```

Headless capture defaults to 3840x2160 and waits three frames before writing the PNG. Use `--page` to select a registered settings page:

```powershell
.\.Build\Preview\dmui-preview.exe --screenshot out.png --page dearmodding.tests.general/results
```

`--width`, `--height`, and `--frames` override the capture defaults. The build copies the theme, fonts, and shaders to `.Build/Preview/Data/F4SE/Plugins/`.

Use `--presentation overlay|notification|image|plot|dialog` to capture a
synthetic client exercising the corresponding public host service.
These fixtures create their resources locally and invoke the public client
wrappers rather than duplicating the host presentation. The `image` fixture
shows a CPU-created checker, the same CPU handle after a deterministic
dimension/color/alpha update, and the existing imported-SRV path together.

Use `--sidebar tree|twopane|drilldown|iconrail` to explicitly override the persisted layout and render any sidebar without rebuilding.
For deterministic tree captures, `--collapse-all` starts with every mod closed and repeatable
`--expand <client-id>` arguments define the exact expanded set. In drill-down, `--collapse-all`
shows the mod root and `--expand <client-id>` opens that mod.

## Generating the stable UI contract

The checked-in DMUI-owned schema is the current contract. The immutable
`ui-contract.manifest.json` records the published ABI-1 baseline and rejects
changes to existing operation IDs, slots, signatures, requirements, and enum
values. Additive slots or enum values require a newer UI revision. Regenerate
the public C table, C++ checked wrappers, and host translation declarations
with:

```powershell
python Depends/commonlibf4/lib/dearmoddingui-api/Tools/generate-ui-contract.py `
  --schema Depends/commonlibf4/lib/dearmoddingui-api/schema/ui-contract.json `
  --baseline-manifest Depends/commonlibf4/lib/dearmoddingui-api/schema/ui-contract.manifest.json `
  --c-header Depends/commonlibf4/lib/dearmoddingui-api/include/DearModdingUI/CUIAPI.h `
  --checked-header Depends/commonlibf4/lib/dearmoddingui-api/include/DearModdingUI/UIChecked.generated.h `
  --host-bindings include/DearModdingUI/UIBindings.generated.h
```

## License

DearModdingUI is licensed under GPL-3.0. See [LICENSE](LICENSE). Third-party software, fonts, provenance, and licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
