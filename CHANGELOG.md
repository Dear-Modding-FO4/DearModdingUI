# Changelog

All notable changes to DearModdingUI will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Localization**: `$KEY` registration text is translated from `Interface\Translations\<client id>_<language>.txt`, falling back to `_en`; `resolveText` translates drawn text.
- **Pointer state**: `getPointerState` reports clicks, wheel input, and cursor position that no DMUI window took to the focused overlay; ImGui does not see those clicks.
- **Translatable host interface**: The host's own text is translated by `DearModdingUI_<language>.txt`, falling back to English.
- **Theme presets**: Themes are TOML files in `Data\F4SE\Plugins\DearModdingUI\Themes` that a mod can ship without rebuilding the host. A preset overrides only the colors, sizes, chrome, effects, typography, and icon glyphs it lists; invalid entries are ignored and reported under `dmui.theme` in Health. `Default.toml` documents every key, and `sMenuThemePreset` or the Settings page selects the preset.
- **Non-Latin text**: Cyrillic, Greek, Chinese, Japanese, and Korean text renders through Windows system fallback fonts, ordered for the game language.
- **Styled text spans**: `ui::Text(text, spans, options)` draws byte ranges of one text that each set foreground and background tones or colors, a font role, bold, italic, underline, and strikethrough, laid out inline with word wrap and an optional bullet as one item. `StyledText` and `TextStyle` compose them; text views and single-line fields take the same spans.
- **Text view interaction**: `drawTextView` takes spans (hover and click events with a caller id), `firstLineNumber` so trimming front lines keeps the reader's place, `FOLLOW_TAIL` that pins to the end only while the reader is there, one-shot scroll commands (page up and down, top, bottom), an `atBottom` output, and `SELECTABLE` pointer selection with Ctrl+C or Ctrl+Insert copy.
- **Styled text fields**: `DMUI_TextFieldOptions::styleSpans` (C++ `TextFieldOptions::styler`) colors and decorates a single-line field after each frame's edits, in the monospace role.
- **Keyboard shortcuts**: `ui::Shortcut(chord, flags)` (`shortcut`) reports a keyboard chord to the page with keyboard focus; an active text field keeps its keys, chords a host hotkey acts on report zero, and `REPEAT` repeats while held.
- **Monospace font**: the monospace role defaults to the bundled JetBrains Mono, with bold and italic faces, and `sMenuMonospaceFontFamily` (a settings row) overrides the family; the built-in vector font remains the `ImGui` family and the fallback.
- **Host button**: `ui::Button(options)` draws the host button with a label and/or theme-resolved Phosphor glyph, tooltip, enabled state, default, primary, or subtle style, and content-fitted or explicit size; `ui::MeasureButton(options)` returns its size for layouts. The host draws its own chrome, dialog, and settings buttons through the same primitive and an internal control renderer chosen by the theme.
- **Item tooltips**: `ui::BeginItemTooltip` and `TooltipScope` follow one host policy: the theme's normal hover delay or navigation focus, also while the item is disabled.
- **One color edit**: `ui::ColorEdit` (`colorEdit`) draws the RGB(A) fields with their picker and an optional row of preset swatches, with packed `0xRRGGBBAA` colors and no label beside it. `ColorSettingControl` adds it to declarative settings pages with a `uint64_t` value, and the host settings view draws its accent and palette colors through it.
- **One key binding editor**: `ui::KeyCapture` (`keyCapture`) captures a chord for a keyboard-and-mouse or gamepad slot, shows a conflict warning, and unbinds, with no label beside it and a read-only form. `KeybindSettingControl` adds it to declarative settings pages with a chord string value. The host settings view and the MCM bridge draw their bindings through it, and its buttons use the host button primitive. Hotkey registration is unchanged.
- **Action lifecycle**: `registerAction` returns a handle; `unregisterAction` (render thread, like `unregisterHotkeyAction`) removes an action and `setActionEnabled` (any thread) greys its toolbar button and hides it from the command palette. `Client::AddAction` returns the handle, with `UnregisterAction` and `SetActionEnabled`.
- **Placement**: `DMUI_Placement` (position and pivot) places `ui::BeginTooltipAt` and the new `ui::BeginPopupAt`, which uses it when the popup appears. `getPointerState` reports `position` and `viewport` on every drawing page, so a tooltip or popup can open at the mouse; clicks and wheel stay with the focused overlay.
- **Cursor and time helpers**: `ui::GetCursorScreenPosX/Y` and `SetCursorScreenPosX/Y`; `ui::GetTime()` and `ui::GetDeltaTime()` report the host frame clock, which advances only on DMUI frames and ignores game pause.
- **Memory images**: `Client::LoadImageMemory` (`loadImageMemory`) loads encoded PNG, JPEG, BMP, GIF, TIFF, or DDS bytes with the file-image decoder, limits, and status behavior. The host copies the bytes before returning.
- **Spinner**: `ui::Spinner` (`spinner`) draws a themed busy indicator, a rotating arc in the accent color, as one item. The host dialog shows it while a submission is working.
- **Translated settings pages**: The built-in `SettingsPage` text (Reset all, Revert, Apply, Modified only, the search hint, tooltips, and notices) comes from the `$DMUI_SettingsPage_*` keys of the host translations, with English fallbacks.

### Changed
- **BREAKING: ABI 3.0**: The host serves only ABI 3 clients; rebuild against the 3.0 headers.
- **Appearance settings are optional overrides**: `sMenuAccentColor`, `fMenuWindowOpacity`, `sMenuPaletteBackgroundColor`, `fMenuPaletteOpacity`, `bMenuMonochromeIcons`, `bMenuBackgroundBlur`, `fMenuBackgroundBlurStrength`, `sMenuLogoColors`, and `sMenuBodyFontFamily` apply on top of the selected theme preset and are written only when set. Existing values in `DearModdingUI.toml` keep applying as overrides; remove a key to follow the preset.
- **MCM bridge**: Translates MCM text through the host's translation files, named after each mod's config folder, instead of the game translator.
- `beginField` / `FieldScope` accept an empty label and a description without a label, matching settings rows.
- Declarative settings pages use the host search input and space groups by the theme's section gap.
- Field feedback and notifications share the status severity colors; the per-severity feedback color settings are removed.
- **BREAKING: one span model**: `DMUI_TextSpan` is a byte range with foreground and background tones or colors, decorations, and a user id; the `textSpans` slot is folded into `text(text, length, spans, count, options)`, `ui::Text(spans, options)` becomes `ui::Text(text, spans, options)`, and `drawTextView`'s descriptor and state grow the span, anchor, flag, scroll, and event fields. A content change no longer resets a text view to the top; send the `TOP` command.
- **BREAKING: styled text**: `DrawStyledText`, `DrawLabeledValue`, `TextStyle`, `ResolveTextColor`, and the `drawBulletText` host slot are replaced by text spans; `TextTone` drops the `kStatus*` duplicates and `SettingsPageNote` carries a tone instead of `muted`.
- **BREAKING: shared vocabulary**: `DMUI_Severity` (Info, Success, Warning, Error) replaces `DMUI_StatusSeverity` and `DMUI_FieldFeedbackSeverity` for status, notifications, diagnostics, and field feedback, which now accepts Success; `dmui::Severity` replaces `FieldFeedbackSeverity` and the raw status constants in `Client`. `DMUI_ThemeTokens` and `ui::GetThemeTokens()` (empty on failure) replace `DMUI_ThemeColors`, `getThemeColors`, `ui::GetThemeColors`, `ui::GetThemeColor`, and `Client::GetThemeColors`. `DearModdingUI::SettingsAction` is gone in favor of `DMUI_SettingsAction`, and the icon and settings-action helpers moved from `DearModdingUI` to `dmui`; host-only row geometry left `Presentation/Layout.h`.
- **BREAKING: one field bracket**: `beginField` / `setFieldFeedback` / `endField` (`FieldScope`) are the only row bracket. `beginSettingsRow`, `beginSettingsRowEx`, `endSettingsRow`, `Client::BeginSettingsRow`, `Client::EndSettingsRow`, `SettingsRowScope`, `DMUI_SettingsRowLayout`, and `DMUI_SettingsRowOptions` are removed.
- **BREAKING: one button and tooltip path**: `drawSettingsActionButton`, `settingsActionButtonWidth`, `settingsActionButtonExtent`, `DMUI_SettingsAction`, and `SettingsActions.h` are removed; Reset, Revert, and Apply are ordinary buttons with a glyph and label, Apply in the primary style. The raw `ui::Button(label, size)`, `ui::SetTooltip`, and `setTooltipText` mirrors are removed, and `TooltipScope` no longer takes hover flags.

- **BREAKING: one text field**: `ui::InputText(id, text, options)` is the only text input. `TextFieldOptions` carries the hint, a `kPlain` or `kSearch` style, multiline height in lines or an explicit size, `maxBytes`, input and edit flags, the reload cursor, and a width. It replaces `inputText`, `inputTextWithHint`, `inputTextMultiline`, the previous `inputTextEditor`, the `drawSearchInput` and `drawSearchInputBuffer` host slots, and `Client::DrawSearchInput`. The ID scopes the field and is never drawn. `TextSettingControl::bufferCapacity` became `maxBytes`, and `TextInputBuffer` is always growable.
- **BREAKING: one section primitive**: `ui::SectionScope` (`beginSection` / `endSection`) draws a titled section with an optional icon (`PhosphorGlyph::kNone` for none, zero to infer from the title), box, count, and caller-owned collapse state. It replaces the `drawSectionHeader` and `drawCollapsingSectionHeader` host slots, `Client::DrawSectionHeader`, `Client::DrawCollapsingSectionHeader`, `DrawDivider`, and `ui::CollapsingHeader` with `DMUI_UITreeNodeFlags`; `HeadingMode::kDivider` is a rule without title. `DMUI_FaqEntry::expanded` makes FAQ disclosure state caller-owned.
- **BREAKING: one choice primitive**: `ui::Choice` (`choice`) is the only dropdown, with per-option tooltips, no label beside it, and an unmatched label that never alters the stored value. It replaces `dmui::DrawChoice`, `ui::BeginCombo`, `ui::EndCombo`, and `DMUI_UIComboFlags`. The host settings dropdowns use it.
- The control renderer also owns the text field frame, section header, rules and box, dropdown rows, checkbox, numeric widgets, color edit and swatches, and the key capture frame.
- **BREAKING: one toggle primitive**: `ui::Toggle` (`toggle`) is the only checkbox and draws no label beside it; the field bracket owns the label. It replaces `ui::Checkbox` (`checkbox`). `CheckboxSettingControl`, the host settings view, and the settings filter use it, and the control renderer owns its look.
- **BREAKING: one numeric primitive**: `ui::Numeric<T>` (`numeric`) is the only numeric input for 8 to 64 bit integers, `float`, and `double`. The presentation (`kSlider`, `kDrag`, or `kInput`) is explicit and no longer inferred from which bounds are set; options carry optional minimum, maximum, and step with origin, a printf-style format the host rebuilds for the data type, a unit suffix, a clamp flag, a width, and a drag speed, and the result reports `changed` and `completed`. It replaces `ui::SliderScalar`, `ui::DragScalar`, `ui::InputScalar` (`sliderScalar`, `dragScalar`, `inputScalar`), `DMUI_UIDataType`, and `DMUI_UISliderFlags`; logarithmic, wraparound, and the other native slider flags are gone. `NumericSettingControl<T>` gains an explicit `presentation`, the MCM bridge picks one from each control's bounds, and the host settings sliders use the primitive.
- **BREAKING: raw text and style color mirrors trimmed**: `ui::TextColored`, `ui::TextDisabled`, `ui::TextWrapped` (`textColored`, `textDisabled`, `textWrapped`), and `ui::PushStyleColor` / `ui::PopStyleColor` (`pushStyleColor`, `pushStyleColorU32`, `popStyleColor`) are removed. Text spans carry tone, color, and wrapping through the theme, while a pushed color bypassed it; `GetStyleColor` and the draw-list color helpers remain.
- **BREAKING: icons are host-only**: `IconGlyphs.h` keeps only the `PhosphorGlyph` constants and `IsRepresentableIconGlyph`; the catalog and resolver (`IconResolver`, `ResolveIconSelection`, `ResolveAutomaticIconGlyph`, and the rest) leave the client headers. Clients resolve names with `Client::ResolveIconGlyph`. The generator moved to `tools\phosphor-icons`.
- **BREAKING: `AddAction`** returns `std::optional<DMUI_ActionHandle>` instead of `bool`, and `SettingFilterOptions::searchHint` and `ChoiceSettingControl::unmatchedLabel` default to empty, which uses the translated built-in text.

### Fixed
- Declarative settings search no longer truncates queries longer than 255 bytes.
- Clients built with newer headers connect to older hosts of the same major; newer-minor operations report `UNSUPPORTED_ABI`.
- Keys DMUI acts on (hotkeys, Escape, controller B) no longer reach the game or type into text fields, and held game actions release when DMUI takes input.

## [0.2.1] - 2026-10-05

### Added
- **Input text editor (ABI 2.1)**: `ui::InputTextEditor` reports per-frame history, completion, submit, cancel, and edit events, along with cursor, selection, and caret screen position. It can reload client-replaced text at a chosen cursor and keep focus after submit, which suits consoles and autocompleting fields. `ui::BeginTooltipAt` places a non-focusable popup, such as a suggestion list, at a screen position.
- **Focused overlays (ABI 2.1)**: `requestOverlayFocus` makes a demanded managed overlay interactive while the shell stays closed, for consoles, palettes, and inspectors. The host blocks game input, pauses unless Fall Souls mode is enabled, and routes keyboard, mouse, and wheel input to the overlay. Escape and controller B leave an active widget, close a client popup, and then end focus, unless an input text editor captures the cancel. One overlay holds focus at a time, and a second request returns `BUSY`. `queryOverlayFocus` reports whether the overlay is still focused and why focus ended: release, cancel, the shell opening, a game interruption, host unavailability, or a failed callback.

### Changed
- **ABI versioning**: The ABI is now versioned as major.minor. Minor versions only append table slots and new types, and the host serves every older minor of its major, so host updates no longer require client rebuilds. Breaking changes and removals are batched into major versions. Existing ABI 2 clients keep working unchanged as ABI 2.0. A client built for a newer minor than the installed host fails to connect, and the host logs both versions. `DMUI_HostAPI::abiVersion` and `DMUI_HostReadyInfo::abiVersion` are renamed `abiMajor`. Clients that compare them in source must compare against `DMUI_ABI_MAJOR`.

## [0.2.0] - 2026-10-03

### Added
- **Managed overlay placement**: The host persists arranged position and size by stable client/page ID; clients supply defaults and can reset placement.
- **Themed panels**: Clipped client panels use the host section tint and provide default page gaps and inner padding, optional scrolling, scoped style-var overrides, and callback recovery.
- **Custom drawing**: Immediate window, foreground, and background draw lists expose geometry, concave polygons, text, images, and callback-isolated clip scopes through ABI 2.
- **File images**: Any-thread `loadImageFile` loads WIC formats and mipmapped DDS asynchronously, with queryable failures, bounded work, safe cancellation, and automatic device reload.
- **Client popups and modals**: Page-scoped popup UI operations and RAII scopes share one modal owner with host dialogs, including nested modals and single-level Escape/controller-B dismissal.
- **Dialog sessions**: `dmui::DialogSession` runs a host confirm or text-entry dialog from one `Open` call and a per-frame `Poll`, keeping it open while submission fails.
- **Host title logo**: Added a resolution-independent Dear Modding vector logo with original colors or an accent-color appearance setting.
- **Controller navigation**: Left-stick and D-pad focus by default; LS or RS toggles a cursor centered on the focused item, which the right stick moves as in the game while the left stick scrolls. A clicks, and the D-pad returns to navigation. Shared B/Escape dismissal, pane switching, scrolling, row reset, search, and compact controller hints are supported. Desktop preview uses XInput.
- **Input bindings**: Host toggle and client hotkeys support keyboard/mouse and gamepad slots, up to three-key or three-button combos, and Ctrl/Alt/Shift modifiers for keyboard/mouse chords. The gamepad menu toggle defaults to LB+RB+View (Back).
- **Shared read-only text viewer**: Added a host-owned, clipped text viewport with independent scrolling, monospace rendering, overlapping literal-match highlighting, wrap navigation helpers, and exact byte-offset reveal.

### Changed
- **Actions**: `AddAction` callbacks run with the same UI context and failure isolation as page callbacks, so they can draw and use the clipboard. C action callbacks now return `DMUI_Result`.
- **Notifications**: Replaced the single banner with attributed, severity-colored toast stacks, bounded queuing, duplicate counts, and hover-paused expiry. The notification descriptor now accepts an optional title.
- **BREAKING: ABI 2**: One exact-match ABI covers all tables and descriptors; clients must rebuild. Replace `Client::DrawImage` with `ui::Image` and `Client::DrawAnnotatedPlot` with `ui::PlotAnnotated`. Removed descriptor sizes, table-prefix/revision negotiation, service bits, and minimum-version client options. The host table exposes its UI table directly.
- **Growable search input**: Restored the three-argument C++ search helper with automatic growth during edits. An optional byte limit remains available; client-owned resize callbacks share the fixed-buffer input mechanism.

### Fixed
- **Periodic frame-time spikes**: Renderer monitoring no longer takes the engine renderer lock while the renderer binding is unchanged. It previously waited up to about 11 ms on the game thread four times per second during gameplay.
- **Menu hitches**: Blur shaders are compiled at build time instead of on first menu open. MCM file-list scans run off the render thread, and closing the menu no longer rewrites an unchanged window layout. Packages no longer ship `Shaders` HLSL files.
- **Search input capacity**: Removed the host's fixed 256-byte temporary buffer. Search controls now honor the caller's explicit UTF-8 byte capacity without partial-sequence truncation.

## [0.1.3] - 2026-09-14

### Added
- **Game color accent synchronization**: Added one-click accent color synchronization in Appearance settings to copy the active HUD (`iHUDColor`) or Pip-Boy (`fPipboyEffectColor`) color configuration from game preferences.
- **Configurable field feedback presentation**: Added configurable field-level feedback presentation for mod settings. Mod pages can provide frame-local Info, Warning, and Error messages on fields and setting rows. Users can configure feedback placement (`sFieldFeedbackLayout`: `strip`, `label`, or `control`) and custom palette colors (`sFieldFeedbackInfoColor`, `sFieldFeedbackWarningColor`, `sFieldFeedbackErrorColor`) in `DearModdingUI.toml` or the in-game Appearance settings.
- **Field layout API**: Extended the host C ABI and C++ client wrapper with `beginField` and `endField` (`FieldScope`), supporting standalone field geometry, full-span row layouts, and declarative setting feedback while maintaining binary compatibility with existing settings row implementations.

### Changed
- **Unified test release distribution**: Consolidated development test packages into a single unified archive containing the host plugin, MCM bridge, diagnostic test client, and test game fixtures.
- **Automatic heading icons**: Expanded the authored icon vocabulary for common interface roles and gameplay sections, including overview, status, character resources, damage, skills, and companions. Specific headings such as *Sleep Tuning* and *Damage Done* select their core subject rather than incidental search tags.

### Fixed
- **Rendering pipeline state preservation**: Preserved D3D11 render targets and Unordered Access Views (UAVs) across background blur passes, restoring bound UAV slots and structure counters to prevent display interference with graphics mods and third-party render hooks.
- **Frame submission synchronization**: Retained frame submissions across failed or deferred `Present` calls and indexed submissions by sequence numbers, preventing delayed present events from prematurely releasing newer frame submissions on the same attachment.
- **Device switching during modal capture**: Preserved gamepad and keyboard/mouse device connect events while the modal menu is open, allowing seamless input device switching during modal input capture.
- **Expandable sidebar rows**: Unified whole-row expand and collapse interactions across origin groups, multi-page mods, and category headers. Toggling a mod row no longer inadvertently triggers page navigation or forces disclosure open; single-page mods and flat mod lists retain direct navigation.

## [0.1.2] - 2026-09-11

### Added
- **Two-pane sidebar layout**: Two-pane navigation now displays mods and their pages side by side in a resizable, independently scrolling two-column layout (`twopane`). The divider position is remembered with window layout.
- **Icon rail sidebar layout**: Added a compact sidebar layout (`iconrail`) featuring an independently scrolling icon rail on the left and mod pages on the right, with hover tooltips displaying mod names.
- **Collapsible origin sections**: Mod origin groups (such as Native and MCM) can now be collapsed or expanded, preserving their disclosure state during navigation.
- **Host-owned icon resolution**: Added the `resolveIconGlyph` host API entry, migrating automatic setting-group icon inference from client heuristics to a centralized host catalog. The host indexes full Phosphor icon names, semantic tags, and domain vocabulary with deterministic whole-word matching, English singularization fallback, and lowest-codepoint tie breaking.

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

[Unreleased]: https://github.com/Dear-Modding-FO4/DearModdingUI/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/Dear-Modding-FO4/DearModdingUI/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/Dear-Modding-FO4/DearModdingUI/compare/v0.1.3...v0.2.0
[0.1.3]: https://github.com/Dear-Modding-FO4/DearModdingUI/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/Dear-Modding-FO4/DearModdingUI/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/Dear-Modding-FO4/DearModdingUI/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/Dear-Modding-FO4/DearModdingUI/releases/tag/v0.1.0
