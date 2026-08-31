# DearModdingUI

DearModdingUI is a standalone F4SE plugin that hosts one shared Dear ImGui menu for Fallout 4 mods. Client plugins discover `DearModdingUI.dll` at runtime and register settings or overlay pages through a versioned C ABI, so clients do not link against the host binary.

The host owns the ImGui context, D3D11 and Win32 backends, common shell, navigation, fonts, theme, cursor, background blur, and host appearance settings. Window layout is stored in `Data/F4SE/Plugins/DearModdingUI/imgui.ini`; appearance settings are stored in `Data/F4SE/Plugins/DearModdingUI.toml`.

## Client registration

The ABI, lifecycle, compatibility fingerprint, and registration examples are documented in [`include/DearModdingUI/README.md`](include/DearModdingUI/README.md). Public client headers also ship in the Dear Modding FO4 CommonLibF4 fork under `include/DearModdingUI/`.

Clients locate the `DMUI_GetHostAPI` export at F4SE `kPostPostLoad`, request the current API version, register the client and all pages, then wait for the host-ready callback before drawing. Clients may open a registered settings page through `selectPage`; the host does not impose a global hotkey.

## Building

Requirements:

- Visual Studio 2022 with the v143 C++ toolset
- [xmake](https://xmake.io/)

```powershell
git clone --recurse-submodules <repository-url>
cd DearModdingUI
xmake build -y
```

The build writes the deployable payload to `.Build/F4SE/Plugins/`, including `DearModdingUI.dll`, `DearModdingUI.toml`, fonts, and blur shaders.

## License

DearModdingUI is licensed under GPL-3.0. See [LICENSE](LICENSE). Third-party software, fonts, provenance, and licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
