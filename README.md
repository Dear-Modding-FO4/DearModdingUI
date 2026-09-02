# DearModdingUI

DearModdingUI is a standalone F4SE plugin that hosts one shared Dear ImGui menu for Fallout 4 mods. Client plugins discover `DearModdingUI.dll` at runtime and register settings or overlay pages through a versioned C ABI, so clients do not link against the host binary.

The host owns the ImGui context, D3D11 and Win32 backends, common shell, navigation, fonts, theme, cursor, background blur, menu toggle key, and host appearance settings. Window layout is stored in `Data/F4SE/Plugins/DearModdingUI/imgui.ini`; host settings are stored in `Data/F4SE/Plugins/DearModdingUI.toml`. The `tree`, `twopane`, or `drilldown` sidebar selected by `[Additional] sMenuSidebarLayout` saves immediately, while cosmetic changes remain previews until Apply.

The host-owned Home page is the landing page for each game launch and summarizes the live client registry. Closing and reopening the menu within that launch returns to the last selected host or client page; active-page selection is not persisted across launches.

## Client registration

The ABI, lifecycle, compatibility fingerprint, and registration examples are documented in [`include/DearModdingUI/README.md`](include/DearModdingUI/README.md). Public client headers live in the standalone DearModdingUI API repository and arrive through CommonLibF4's `lib/dearmoddingui-api` public dependency.

Clients locate the `DMUI_GetHostAPI` export at F4SE `kPostPostLoad`, request the current API version, register the client and all pages, then wait for the host-ready callback before drawing. Clients may open a registered settings page through `selectPage`; the host opens and closes the shared menu with `[Additional] sMenuToggleKey`, which defaults to F11.

## Building

Requirements:

- Visual Studio 2022 with the v143 C++ toolset
- [xmake](https://xmake.io/)

```powershell
git clone <repository-url>
cd DearModdingUI
git submodule update --init
git -C Depends/commonlibf4 submodule update --init --recursive
xmake build -y
```

The build writes the deployable payload to `.Build/F4SE/Plugins/`, including `DearModdingUI.dll`, `DearModdingUI.toml`, fonts, and blur shaders.

## Standalone preview

The `dmui-preview` target runs the production menu renderer in its own Win32/D3D11 window with representative fake clients:

```powershell
xmake build -y dmui-preview
.\.Build\Preview\dmui-preview.exe
```

Headless capture defaults to 3840x2160 and waits three frames before writing the PNG. Use `--page` to select a registered settings page:

```powershell
.\.Build\Preview\dmui-preview.exe --screenshot out.png --page dearmodding.addictol/settings
```

`--width`, `--height`, and `--frames` override the capture defaults. The build copies the theme, fonts, and shaders to `.Build/Preview/Data/F4SE/Plugins/`.

Use `--sidebar tree|twopane|drilldown|iconrail` to explicitly override the persisted layout and render any sidebar without rebuilding.
For deterministic tree captures, `--collapse-all` starts with every mod closed and repeatable
`--expand <client-id>` arguments define the exact expanded set. In drill-down, `--collapse-all`
shows the mod root and `--expand <client-id>` opens that mod.

## Generating the client ImGui header

The forwarding surface is curated in `Tools/imgui_forward_allowlist.json`. Generate the API repository header and validate every referenced symbol against the built host with:

```powershell
python Tools/generate_imgui_forward.py --definitions Depends/cimgui/generator/output/definitions.json --allowlist Tools/imgui_forward_allowlist.json --output ../DearModdingUI-API/include/DearModdingUI/ImGuiForward.h --dll .Build/F4SE/Plugins/DearModdingUI.dll --dumpbin <path-to-dumpbin.exe>
```

## License

DearModdingUI is licensed under GPL-3.0. See [LICENSE](LICENSE). Third-party software, fonts, provenance, and licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
