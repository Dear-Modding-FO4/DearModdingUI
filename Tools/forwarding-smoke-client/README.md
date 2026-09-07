# DearModdingUI forwarding smoke client

**Development-only manual harness.** It is not part of the normal build or
release package. It registers nothing when the DearModdingUI host is absent or
does not provide the required forwarding version/services, and logs that
outcome once. It has no standalone UI, fallback menu, automation, game launch,
input injection, or disk persistence.

## Isolated opt-in build

Run this block from the root of your checkout in one PowerShell process. Resolve
the absolute project path explicitly and keep CommonLib's automatic install
destination inside that checkout:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
$env:FO4_DEV_MODS = Join-Path $projectRoot '.Build\isolated-mods'
Remove-Item Env:XSE_FO4_MODS_PATH -ErrorAction SilentlyContinue
Remove-Item Env:XSE_FO4_GAME_PATH -ErrorAction SilentlyContinue
xmake f -P "$projectRoot" -m release --forwarding_smoke=y
xmake build -P "$projectRoot" -y dmui-forwarding-smoke
```

Artifacts are limited to:

- `.Build\ForwardingSmoke\dmui-forwarding-smoke.dll`
- `.Build\ForwardingSmoke\dmui-forwarding-smoke.pdb`
- local linker support files in `.Build\ForwardingSmoke` (`.lib`, `.exp`,
  `.ilk`, and the compile PDB)
- `.Build\isolated-mods\Dear Modding UI - Forwarding Smoke Test\F4SE\Plugins\`
  containing the staged DLL/PDB

Do not run `xmake install`, point an XSE variable at a game/live mod manager, or
launch the game from build tooling.

## Manual setup and checklist

1. Copy or enable the isolated staged smoke-test folder in a disposable manual
   development profile. Enable the separately built DearModdingUI host before
   startup. The smoke client performs its single host discovery/registration
   attempt at F4SE `kPostPostLoad`, after all plugins have loaded; it does not
   retry later or on Present.
2. Start the game manually. Open the host and select **Forwarding Smoke Test
   (Development Only)**. Verify host state is ready, API is 0.1, forwarding is
   1.1, and all required service bits are reported. If the host was absent or
   incompatible at `kPostPostLoad`, verify the smoke client logged once and
   registered nothing.
3. With the host menu closed, use `Ctrl+Shift+F10` to toggle the overlay and
   `Ctrl+Shift+F11` to schedule the bounded delayed toast. Verify movement and
   gameplay input remain unaffected.
4. Exercise menu, console, text-entry, focus-loss, and other obstructed
   contexts. The gameplay defaults must yield. Bind the default-NONE
   `HOST_INPUT_INACTIVE`, optional `ALWAYS`, letter `A`, and digit `7` probes
   in the host hotkey manager; toggle their enable state on the smoke page and
   verify effective binding/conflict plus press/release counters.
5. Verify the overlay timer and observer samples continue while the overlay
   and host menu are hidden. Try all anchors, free X/Y, scale, opacity, and
   arrangement. Free movement must work only while the host menu owns input;
   verify placement generation and arrangement-completed count.
6. Verify the immutable checker/gradient appears. Use **Cycle / recreate
   image**, then **Release after queued draw**; the queued draw should survive
   release and one replacement should be imported. Renderer/device changes
   should invalidate and recreate without growing resources.
7. Edit native text, slider, and multiline controls. Verify live changed and
   completed counters, effective per-row reset, simulated-save counts only on
   completed changed edits, and no count increase from unchanged clicks.
8. Post the page notification and delayed any-thread notification repeatedly;
   only one delayed job may be in flight.
9. Accept and cancel the harmless confirm dialog. The operation count changes
   only after accepted `COMPLETED`. Close the menu while it says `SUBMITTED`;
   observer polling must still finish it.
10. Submit empty text, `reject`, and duplicate `alpha`. Verify non-empty errors
    and preserved text. Toggle **Reject with nullptr error** and repeat to
    verify the no-message rejection choice. Submit a unique name and verify one
    completion per unique submission ID.
11. Check the annotated frame-time plot label remains visible while samples
    and reference lines are clipped to the plot area.

## Diagnostic logging

The smoke client writes bounded, event-driven diagnostics to
`Documents\My Games\Fallout4\F4SE\dmui-forwarding-smoke.log`. It records the
one-time initialization and service preflight, each hotkey's registration and
initial effective binding, hotkey edges, overlay frame-demand changes, image
lifecycle transitions, notification scheduling/posting, completed edits and
resets, and dialog state transitions. It never logs entered text and does not
log ordinary per-frame queries or draws.

Use **Log current results** on the smoke settings page for an on-demand compact
snapshot. A second compact snapshot is written automatically on the active to
inactive edge when that exact settings page is left or closed. Snapshot outcomes
are labeled `observed` or `unexercised`; unexercised probes remain unexercised,
and a snapshot never treats the absence of errors as an overall pass.

These checks validate the forwarding API harness. They do **not** validate a
consumer's proxy-swapchain attachment or renderer-replacement integration.

After the manual test, remove or disable all installed smoke-test artifacts but
retain this source. Track that operational step under coordinator TODO
`forwarding-smoke-cleanup`.
