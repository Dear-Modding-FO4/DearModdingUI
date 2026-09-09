# Contributing to DearModdingUI

DearModdingUI is a standalone F4SE plugin hosting a shared Dear ImGui overlay for Fallout 4 mods. Mod plugins integrate through a versioned C ABI without linking against the host binary or compiling Dear ImGui code.

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

For diagnostic builds with the in-game test client, configure with `--test-release=y`.

## Repository layout

| Path | Purpose |
|---|---|
| `src/`, `include/` | Host plugin implementation and internal headers. |
| `mcm/src/`, `mcm/include/` | Game-independent MCM JSON parser, mapping logic, and bindings. |
| `mcm/runtime/` | MCM bridge plugin entry point and game adapters. |
| `tests/` | Automated test suite and regression checks. |
| `tests/fixtures/` | Reusable mock clients and synthetic UI scenarios. |
| `tools/preview/` | Standalone desktop preview application (`dmui-preview`). |
| `tools/test-client/` | Thin F4SE runner for diagnostic client verification. |

## Running tests

Run the test suite locally with xmake:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
xmake build -P "$projectRoot" -y dmui-tests
.\.Build\Tests\dmui-tests.exe
```

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

## Stable UI contract

DearModdingUI exposes a stable C UI table generated from `schema/ui-contract.json`. The baseline manifest `schema/ui-contract.manifest.json` guarantees backward compatibility by verifying existing slots, IDs, and enums remain intact.

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
- Commits use `type(scope): summary` convention.
- Avoid em-dashes in documentation and strings; use clean punctuation or parentheses instead.
- Fail closed: Invalid or malformed client data must be skipped safely with diagnostic logging, never causing a crash.
