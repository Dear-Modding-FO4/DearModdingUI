# Contributing to DearModdingUI

## Prerequisites

- Windows 10 or 11 (x64)
- Visual Studio 2022 with the v143 C++ build tools
- [xmake](https://xmake.io/)
- Python 3 (for contract generation and script tests)
- Git with submodule support

## Setting up

Clone the repository recursively:

```powershell
git clone --recurse-submodules https://github.com/Dear-Modding-FO4/DearModdingUI.git
cd DearModdingUI
```

If you already cloned without submodules, initialize them:

```powershell
git submodule update --init
git -C Depends/commonlibf4 submodule update --init --recursive
```

## Building

Configure and build the release binaries:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
xmake f -P "$projectRoot" -m release --test-release=n -y
xmake build -P "$projectRoot" -y
xmake package-release -P "$projectRoot"
```

Release builds produce two installable packages in `.Build/packages/`:

| Package | Description |
|---|---|
| `DearModdingUI-<version>-release.zip` | Core host DLL, configuration, and fonts. |
| `DearModdingUI-MCM-<version>-release.zip` | Optional bridge DLL for legacy MCM menus. Requires the host. |

Package versions come from `plugin_version` in `xmake.lua`.
Packaging reads runtime assets from the working tree under `data\F4SE\Plugins`.

## Local fixtures

The test plugin, preview scenarios, showcase, and fixture-only tests live in the
gitignored `fixtures\` folder. When present, xmake includes them in local builds.
With `--test-release=y`, packaging produces one `DearModdingUI-<version>-test.zip`
containing the host, MCM bridge, test plugin, and test data. This option requires
local fixtures. CI and public releases do not use them.

## Editor integration

Optional project generation, run from the repository root:

| Editor | Command | Output |
|---|---|---|
| Visual Studio | `xmake project -k vsxmake` | `vsxmakeXXXX\` solution directory. |
| clangd | `xmake project -k compile_commands` | `compile_commands.json`. |

## Repository layout

| Path | Purpose |
|---|---|
| `src\DearModdingUI\`, `include\DearModdingUI\` | Matching `host`, `controls`, `navigation`, `pages`, `settings`, `presentation`, and `localization` components. |
| `src\Platform\`, `include\Platform\` | Native input, renderer attachment, external-file, system-font, and game-text adapters. |
| `src\Support\`, `include\Support\` | Shared runtime, bounded-string, task, and health primitives. |
| `mcm\src\`, `mcm\include\` | Game-independent configuration decoding, mapping, and bindings. |
| `mcm\adapters\` | Stable-UI rendering adapters shared by MCM consumers. |
| `mcm\runtime\` | MCM bridge plugin entry point and game adapters. |
| `tests\` | Subsystem suites under `host`, `presentation`, `platform`, and `mcm`; typed helpers under `support`. |
| `tools\preview\` | Desktop host preview, capture support, and preview-only navigation implementations. |
| `tools\build-support\include\` | Compatibility stubs used only by tests and preview builds. |

`xmake.lua` declares shared source sets once for every target.

## Running tests

Run the test suite locally with xmake:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
xmake build -P "$projectRoot" -y dmui-tests
.\.Build\Tests\dmui-tests.exe
```

## Standalone preview

The standalone preview runs the host UI in a desktop window without launching
Fallout 4. Without local fixtures, it shows only host pages:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
xmake build -P "$projectRoot" -y dmui-preview
.\.Build\Preview\dmui-preview.exe
```

Useful arguments:

- `--screenshot <path>`: Captures a PNG image headlessly and exits.
- `--scroll-y <pixels>`: Scrolls page content before capture.
- `--page <id>`: Navigates directly to a registered settings page.
- `--sidebar <tree|twopane|drilldown|iconrail>`: Selects a sidebar presentation layout.
- `--presentation <overlay|notification|image|plot|dialog|modal|popup|draw-list>`: Activates a local fixture scenario.
- `--menu-closed`: Hides the shell for overlay-only notification captures.
- `--frames <count>`: Renders more frames before capture if a scenario needs time to initialize.

With local fixtures present, for example:

```powershell
.\.Build\Preview\dmui-preview.exe --page text-view/reader --frames 12 --screenshot .Build\Preview\TextViewReader.png
```

## Stable UI contract

The API dependency owns `schema\ui-contract.json` and its baseline manifest,
`schema\ui-contract.manifest.json`. They generate the stable C UI table and check
the UI table and `API.h` against the published ABI version: additions require a
`DMUI_ABI_MINOR` bump, and changes or removals require a `DMUI_ABI_MAJOR` bump.

To regenerate contract bindings after updating the schema:

```powershell
$api = 'Depends/commonlibf4/lib/dearmoddingui-api'
python "$api/Tools/generate-ui-contract.py" `
  --schema "$api/schema/ui-contract.json" `
  --baseline-manifest "$api/schema/ui-contract.manifest.json" `
  --c-header "$api/include/DearModdingUI/CUIAPI.h" `
  --checked-header "$api/include/DearModdingUI/UIChecked.generated.h" `
  --host-bindings include/DearModdingUI/UIBindings.generated.h
```

Use `--update-baseline` to record a new version's manifest, or to refresh an
unreleased minor during development. Never edit
the generated headers directly: CI regenerates and compares them.

## Guidelines

- Use American English in code, comments, and documentation.
- Use PascalCase for first-party source, header, script, and authored asset filenames, preserving established acronyms such as `MCM` and `ImGui`. Conventional tool/metadata names, generated filenames, upstream resources, and externally defined runtime paths are exceptions. Use lowercase directory names within components.
- Group implementations and internal headers by responsibility. Keep implementation-private headers with their component; use a consistent include path for headers shared across components.
- Remove dead code and forwarding layers that add no behavior. Put reused logic in its owning component rather than copying it or adding a miscellaneous utility collection.
- Treat a handwritten file exceeding 1,000 lines as an ownership warning, not a reason to split it arbitrarily. Extract cohesive responsibilities and keep their state with them.
- Keep game-independent MCM logic separate from game adapters, and diagnostic-only implementations out of production source sets.
- Test behavior through production code, including failure paths, rather than snapshots of defaults, fixtures, or source text.
- Commits use `type(scope): summary` convention.
- Avoid em-dashes in documentation and strings; use clean punctuation or parentheses instead.
- Fail closed: Invalid or malformed client data must be skipped safely with diagnostic logging, never causing a crash.
