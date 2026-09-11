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

The build produces two installable packages in `.Build/packages/`:

| Package | Description |
|---|---|
| `DearModdingUI-<version>-release.zip` | Core host DLL, configuration, shaders, and fonts. |
| `DearModdingUI-MCM-<version>-release.zip` | Optional bridge DLL for legacy MCM menus. Requires the host. |

Package versions come from `plugin_version` in `xmake.lua`.
For diagnostic builds with the in-game test client, configure with `--test-release=y`.
Packaging reads runtime assets from the working tree under `data\F4SE\Plugins`.

## Repository layout

| Path | Purpose |
|---|---|
| `src\DearModdingUI\`, `include\DearModdingUI\` | Matching `host`, `controls`, `navigation`, `pages`, `settings`, and `presentation` components. |
| `src\Platform\`, `include\Platform\` | Native input, renderer attachment, and external-file adapters. |
| `src\Support\`, `include\Support\` | Shared runtime, bounded-string, task, and health primitives. |
| `mcm\src\`, `mcm\include\` | Game-independent configuration decoding, mapping, and bindings. |
| `mcm\adapters\` | Stable-UI rendering adapters shared by MCM consumers. |
| `mcm\runtime\` | MCM bridge plugin entry point and game adapters. |
| `tests\` | Subsystem suites under `host`, `presentation`, `platform`, and `mcm`; typed helpers under `support`. |
| `tests\fixtures\` | Reusable synthetic client descriptors and registration fixtures. |
| `tools\preview\` | Desktop preview, capture support, synthetic scenarios, and preview-only navigation implementations. |
| `tools\shared\` | One interactive diagnostic suite shared by preview and the in-game runner. |
| `tools\test-client\` | Thin F4SE runner for the shared diagnostic suite. |

`xmake.lua` declares shared source sets once and compiles them with each target's
own defines and adapters. Only desktop preview and automated tests use
`tools\shared\include` compatibility stubs; game plugins use the real dependencies.

## Running tests

Run the test suite locally with xmake:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
xmake build -P "$projectRoot" -y dmui-tests
.\.Build\Tests\dmui-tests.exe
```

Prefer behavior exercised through production code over snapshots of release
versions, visual defaults, fixture data, or source text. Preserve ABI contracts,
ownership, state transitions, failure paths, and real I/O boundaries. Consolidate
overlapping setup without dropping distinct failure cases, and name tests for the
path they actually exercise rather than implying host integration through a fake.

The optional `python tests\RunMutations.py` command proves the named regression
controls in `tests\Mutations.json`. It temporarily edits exact source locations,
rebuilds, and restores their original contents. Run it only without concurrent
editing or builds.

## Standalone preview

The standalone preview runs the UI renderer in a desktop window with synthetic data, allowing you to iterate on UI components without launching Fallout 4:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
xmake build -P "$projectRoot" -y dmui-preview
.\.Build\Preview\dmui-preview.exe
```

Useful arguments:

- `--screenshot <path>`: Captures a PNG image headlessly and exits.
- `--page <id>`: Navigates directly to a registered settings page.
- `--sidebar <tree|twopane|drilldown|iconrail>`: Selects a sidebar presentation layout.
- `--presentation <overlay|notification|image|plot|dialog>`: Tests specific presentation services.

Presentation presets use the shared exercises in `tools\shared`; MCM and navigation
scenarios live in `tools\preview\fixtures`. Preview binaries and fixtures are not packaged.
Increase `--frames` if capture fails because a scenario has not finished initializing.

## Stable UI contract

The API dependency owns `schema\ui-contract.json` and its baseline manifest,
`schema\ui-contract.manifest.json`. They generate the stable C UI table and check
that existing slots, IDs, and enums remain intact.

An API update must not disconnect a mod that only uses unchanged operations.
Preserve existing function signatures and table offsets, append new operations,
and negotiate optional entries by table size and availability. Release-version
metadata is not a compatibility gate. Internal refactors do not require an ABI
version bump; incompatible operation revisions need distinct entries rather
than repurposing an existing slot.

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

## Guidelines

- Use American English in code, comments, and documentation.
- Use PascalCase for first-party source, header, script, and authored asset filenames, preserving established acronyms such as `MCM` and `ImGui`. Conventional tool/metadata names, generated filenames, upstream resources, and externally defined runtime paths are exceptions. Use lowercase directory names within components.
- Group implementations and internal headers by responsibility. Keep implementation-private headers with their component; use a consistent include path for headers shared across components.
- Remove dead code and forwarding layers that add no behavior. Put reused logic in its owning component rather than copying it or adding a miscellaneous utility collection.
- Treat a handwritten file exceeding 1,000 lines as an ownership warning, not a reason to split it arbitrarily. Extract cohesive responsibilities and keep their state with them.
- Keep game-independent MCM logic separate from game adapters, and diagnostic-only implementations out of production source sets.
- Commits use `type(scope): summary` convention.
- Avoid em-dashes in documentation and strings; use clean punctuation or parentheses instead.
- Fail closed: Invalid or malformed client data must be skipped safely with diagnostic logging, never causing a crash.
