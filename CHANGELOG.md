# Changelog

All notable changes to DearModdingUI will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2] - 2026-09-11

### Added
- **Two-pane sidebar layout**: Two-pane navigation now displays mods and their pages side by side in a resizable, independently scrolling two-column layout (`twopane`). The divider position is remembered with window layout.
- **Icon rail sidebar layout**: Added a compact sidebar layout (`iconrail`) featuring an independently scrolling icon rail on the left and mod pages on the right, with hover tooltips displaying mod names.
- **Collapsible origin sections**: Mod origin groups (such as Native and MCM) can now be collapsed or expanded, preserving their disclosure state during navigation.
- **Host-owned icon resolution**: Added the `resolveIconGlyph` host API entry (`DMUI_HOST_API_RESOLVE_ICON_GLYPH_SIZE`), migrating automatic setting-group icon inference from client heuristics to a centralized host catalog. The host indexes full Phosphor icon names, semantic tags, and domain vocabulary with deterministic whole-word matching, English singularization fallback, and lowest-codepoint tie breaking.

### Changed
- **Fallout 4 native menu cursor**: Replaced software and OS cursor rendering with Fallout 4's native hardware menu cursor during modal menu display, matching in-game cursor appearance, speed, and scaling.
- **Cursor and platform isolation**: Isolated ImGui Win32 window message handling and cursor ownership from game state to prevent mouse capture conflicts and cursor leaks.
- **Tree layout refinement**: Single-page mods now open directly without rendering redundant nested child page rows or disclosure arrows.
- **Subtle disclosure arrows**: Navigation tree disclosure markers use reduced scaling (0.6x) and muted text coloring, rendering only when hovered, focused, active, or expanded.
- **Visual centering**: Icon glyphs in sidebar rows and the icon rail are centered.
- **Appearance defaults**: Increased default window background opacity (`fMenuWindowOpacity`) from 0.55 to 0.75 and default background blur strength (`fMenuBackgroundBlurStrength`) from 0.30 to 0.50. Existing user configurations in `DearModdingUI.toml` and `imgui.ini` are preserved across updates; new defaults can be restored with the in-menu Reset button.
- **Focus handling**: Modal menu remains visible on window focus loss while suspending input interception. Modal rendering occurs before the native cursor pass on the attached backbuffer.
- **MCM user keybind snapshot**: Global user keybind configuration (`Keybinds.json`) is read once per discovery pass as an immutable snapshot across discovered mods.
- **Internal modularization**: Reorganized monolithic source files across the host, presentation, platform, settings, and MCM parser into focused component modules (`host`, `controls`, `navigation`, `pages`, `settings`, `presentation`, `Platform/rendering`, `Platform/input`).
- **Test suite reorganization**: Restructured tests into domain-specific suites (`tests/host`, `tests/mcm`, `tests/platform`, `tests/presentation`, `tests/support`), adding tests for cursor ownership, Win32 integration, and icon resolution. Factored synthetic preview and test-client exercises into `tools/shared`.

### Fixed
- **OG runtime stability (Fallout 4 1.10.163)**: Resolved crashes in the MCM bridge and fixed settings persistence failures on OG runtimes.
- **OG Papyrus callable marshaling**: Fixed marshaling for Papyrus callables on OG runtime virtual machines.
- **Runtime-aware menu visibility**: Updated menu visibility and input interception to query runtime-specific UI and timer layout accessors across OG, NG, and AE game versions.
- **Collapsible row hover consistency**: Unified hover and interaction highlight states on collapsible controls.
- **Redundant captions**: Removed redundant origin captions from individual mod rows when categorized under origin headings.

### Removed
- **Scaleform context probe**: Removed experimental Scaleform spike implementation and test state from the MCM bridge.

### Client Compatibility Notes
- **Automatic icon resolution rebuild**: Client plugins using automatic `SettingGroup` headers (zero glyph) must rebuild once against the updated C ABI (`dearmoddingui-api` commit `9cfc079` / CommonLibF4 `987aedc8`) to query `resolveIconGlyph`. Subsequent host vocabulary improvements are host-only updates and require no further client rebuilds.
- **Host version requirement**: Clients calling `resolveIconGlyph` require DearModdingUI host >= 0.1.2. Older hosts lacking the appended table entry reject the call, and host callback isolation disables the calling page to prevent invalid draw operations.
- **UI contract migration**: Clients using legacy ImGui forwarding must continue migrating to the stable, versioned `dmui::ui` contract (`Client.h`).

## [0.1.1] - 2026-09-09

### Fixed
- Fixed renderer compatibility on Fallout 4 OG (1.10.163) runtimes.
- Fixed MCM menu localization and shared settings rendering.
- Reported transient swapchain attachment failures cleanly.

### Changed
- Consolidated automated test suites and behavioral coverage.
- Updated documentation and added the CONTRIBUTING guide.

## [0.1.0] - 2026-09-09

### Added
- Initial release of DearModdingUI standalone F4SE host plugin.
- Shared in-game Dear ImGui overlay hosted via a versioned C ABI.
- Navigation with Tree and Drill-down sidebar browsing layouts.
- Dynamic theme styling, Phosphor typography, and D3D11 background blur shaders.
- Live host subsystem and client health monitoring.
- Optional `DearModdingUI-MCM` bridge translating legacy MCM JSON configurations.
- In-game diagnostic test client and standalone desktop UI preview (`dmui-preview`).

[0.1.2]: https://github.com/Dear-Modding-FO4/DearModdingUI/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/Dear-Modding-FO4/DearModdingUI/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/Dear-Modding-FO4/DearModdingUI/releases/tag/v0.1.0
