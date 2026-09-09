<div align="center">

<img src="assets/cover.jpg" alt="DearModdingUI" width="640">

# DearModdingUI

**Shared Dear ImGui settings host and overlay system for Fallout 4 mods.**

DearModdingUI provides one shared in-game settings menu for Fallout 4 mods.
Mod plugins register their own pages and controls into a unified interface,
eliminating the need for separate menus and disjointed hotkeys.

<br>

[![CI](https://img.shields.io/github/actions/workflow/status/Dear-Modding-FO4/DearModdingUI/xmake.yml?branch=main&style=for-the-badge&label=CI&logo=githubactions&logoColor=white)](https://github.com/Dear-Modding-FO4/DearModdingUI/actions/workflows/xmake.yml)
[![Release](https://img.shields.io/github/v/release/Dear-Modding-FO4/DearModdingUI?style=for-the-badge&label=release&color=orange)](https://github.com/Dear-Modding-FO4/DearModdingUI/releases/latest)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue?style=for-the-badge)](LICENSE)

[![Fallout 4](https://img.shields.io/badge/Fallout%204-OG%20%C2%B7%20NG%20%C2%B7%20AE-3a7d44?style=for-the-badge)](#requirements)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](xmake.lua)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D6?style=for-the-badge&logo=windows&logoColor=white)](CONTRIBUTING.md#prerequisites)

<sub>[Requirements](#requirements) · [Features](#features) · [Installation](#installation) · [Configuration](#configuration) · [Menu Controls](#menu-controls) · [For Developers](#for-mod-developers) · [Contributing](#contributing) · [License](#license)</sub>

</div>

---

## Requirements

| Component | Requirement |
|---|---|
| **Fallout 4** | OG **1.10.163**, NG **1.10.984**, or AE **1.11.240**. One binary supports all three runtimes. |
| **[Fallout 4 Script Extender (F4SE)](https://f4se.silverlock.org/)** | Matching version for your game runtime. |
| **[Address Library for F4SE](https://www.nexusmods.com/fallout4/mods/47327)** | Matching database for your game runtime. |

---

## Features

| Capability | Details |
|---|---|
| **Unified menu** | A single in-game interface where participating mods present settings and status. |
| **Modern UI and theming** | Hardware-accelerated Dear ImGui rendering with custom styling, typography, and background blur. |
| **Flexible navigation** | Tree, two-pane, and drill-down sidebar browsing layouts with mod category grouping and live search. |
| **Live health monitoring** | Real-time observation of renderer status, configuration persistence, input interception, and typography. |
| **Zero runtime link dependency** | Mod plugins interact via a versioned C ABI. Clients do not link against the host DLL or bundle Dear ImGui. |
| **Legacy MCM support** | An optional bridge converts existing MCM configurations into DearModdingUI pages automatically. |

---

## Installation

Install using your preferred mod manager (Mod Organizer 2, Vortex), or extract the archive into the Fallout 4 `Data` folder:

```text
Data\
└─ F4SE\Plugins\
   ├─ DearModdingUI.dll
   ├─ DearModdingUI.toml
   └─ DearModdingUI\ (fonts, shaders, imgui.ini)
```

To enable support for classic MCM-based mods, install the companion package:

```text
Data\
└─ F4SE\Plugins\
   └─ DearModdingUI-MCM.dll
```

Download the latest packages from the [Releases](https://github.com/Dear-Modding-FO4/DearModdingUI/releases/latest) page.

The build and package version comes from `plugin_version` in [xmake.lua](xmake.lua).
The release badge above tracks the latest published release, which may lag the
version in the source checkout.

---

## Configuration

Host settings are stored in `Data/F4SE/Plugins/DearModdingUI.toml`. Window state and layout are preserved in `Data/F4SE/Plugins/DearModdingUI/imgui.ini`.

```toml
[Menu]
# Hotkey to toggle the menu (End, Home, Insert, Delete, F1-F12)
sMenuToggleKey = "End"

[Interface]
# Sidebar presentation style (tree, twopane, drilldown)
sMenuSidebarLayout = "tree"
```

---

## Menu Controls

- **Toggle Menu**: Press **End** (or your configured key) to open and close the interface.
- **Dismiss Popups / Close**: Press **Escape** to cancel active edits, dismiss open dialogs, or close the menu.
- **Built-in Host Pages**:
  - **Home**: Overview of active mod registrations and system health.
  - **Health**: Diagnostic breakdown of host subsystems and per-client status.
  - **Settings**: Visual appearance, theme colors, and layout preferences.

---

## For Mod Developers

To integrate your F4SE plugin with DearModdingUI:

1. Consume the public API headers from the [DearModdingUI-API](https://github.com/Dear-Modding-FO4/DearModdingUI-API) repository (included automatically in the Dear Modding CommonLibF4 fork).
2. Discover `DearModdingUI.dll` at F4SE `kPostPostLoad` via `DMUI_GetAPI`.
3. Register your client descriptor and pages using the lightweight, header-only C++ wrapper (`<DearModdingUI/Client.h>`).
4. Draw controls using the stable `dmui::ui` namespace without compiling Dear ImGui into your plugin.

Refer to [`include/DearModdingUI/README.md`](include/DearModdingUI/README.md) and the [API documentation](https://github.com/Dear-Modding-FO4/DearModdingUI-API) for detailed integration guides.

---

## Contributing

Contributions, bug reports, and enhancements are welcome.
For development environment setup, build instructions, test execution, standalone preview tools, and contract code generation, see [CONTRIBUTING.md](CONTRIBUTING.md).

---

## License

DearModdingUI is licensed under GPL-3.0. See [LICENSE](LICENSE). Third-party software notices and font licenses are detailed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).