<div align="center">

<img src="assets/Cover.jpg" alt="DearModdingUI" width="640">

# DearModdingUI

**A shared Dear ImGui settings menu and overlay host for Fallout 4 mods.**

[![CI](https://img.shields.io/github/actions/workflow/status/Dear-Modding-FO4/DearModdingUI/xmake.yml?branch=main&style=for-the-badge&label=CI&logo=githubactions&logoColor=white)](https://github.com/Dear-Modding-FO4/DearModdingUI/actions/workflows/xmake.yml)
[![Release](https://img.shields.io/github/v/release/Dear-Modding-FO4/DearModdingUI?style=for-the-badge&label=release&color=orange)](https://github.com/Dear-Modding-FO4/DearModdingUI/releases/latest)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue?style=for-the-badge)](LICENSE)

</div>

## Requirements

| Component | Requirement |
|---|---|
| **Fallout 4** | OG **1.10.163**, NG **1.10.984**, or AE **1.11.240**. One binary supports all three runtimes. |
| **[Fallout 4 Script Extender (F4SE)](https://f4se.silverlock.org/)** | Matching version for your game runtime. |
| **[Address Library for F4SE](https://www.nexusmods.com/fallout4/mods/47327)** | Matching database for your game runtime. |

## Installation and use

Install the core package from [Releases](https://github.com/Dear-Modding-FO4/DearModdingUI/releases/latest)
with Mod Organizer 2 or Vortex, or extract it into Fallout 4's `Data` folder.
Add the optional **DearModdingUI-MCM** companion package for legacy MCM menus.
Use `release` packages for normal play. The [test bundle](tools/test-client/README.md)
includes native and MCM fixtures for a disposable save.

Press **End** to toggle the menu. **Escape** cancels an active edit, dismisses a
dialog, or closes the menu. The built-in Settings page controls appearance and
sidebar layout; Health provides host and client diagnostics.

| File under `Data\F4SE\Plugins\` | Purpose |
|---|---|
| `DearModdingUI.toml` | Host settings, including the menu toggle key. |
| `DearModdingUI\imgui.ini` | Saved window state and layout. |

Preserve both files when upgrading.

## Architecture

| Component | Responsibility |
|---|---|
| **Host** | Shared navigation, controls, rendering, native cursor/input ownership, fonts, and icons. |
| **Client API** | Versioned C ABI with a header-only C++ wrapper. Clients neither link the host DLL nor bundle Dear ImGui. |
| **MCM bridge** | Optional legacy configuration and game adapters. The host links no MCM or JSON code. |
| **Diagnostics** | One shared interactive suite used by the desktop preview and in-game test client. |

See the [repository layout](CONTRIBUTING.md#repository-layout) and
[MCM contracts](mcm/README.md) for component boundaries.

## Mod integration

Use the [public API headers](https://github.com/Dear-Modding-FO4/DearModdingUI-API),
also included in the Dear Modding CommonLibF4 fork. Discover `DearModdingUI.dll`
via `DMUI_GetAPI` at F4SE `kPostPostLoad`, register clients and pages through
`<DearModdingUI/Client.h>`, and draw controls with `dmui::ui`.

The [client guide](include/DearModdingUI/README.md) covers registration,
optional API negotiation, settings, overlays, and callback lifetimes.

## Development

Building requires **Windows 10/11 x64**, **Visual Studio 2022 with v143 C++ tools**,
**[xmake](https://xmake.io/)**, **Python 3**, and **Git with initialized submodules**.
The project uses C++23.

[CONTRIBUTING.md](CONTRIBUTING.md) covers setup, builds, packaging, editor
integration, tests, the standalone preview, and contract generation.

## License

Licensed under [GPL-3.0](LICENSE). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
for dependency and font licenses.