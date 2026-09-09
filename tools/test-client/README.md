# DearModdingUI test client

**Test-release manual harness.** It is not part of the release build or package.
It registers nothing when the DearModdingUI host is absent or
does not provide the required stable UI table/services, and logs that
outcome once. It has no standalone UI, fallback menu, automation, game launch,
input injection, or disk persistence.

## Test-release build

Run this block from the root of the checkout:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
xmake f -P "$projectRoot" -m release --test-release=y
xmake build -P "$projectRoot" -y
xmake package-release -P "$projectRoot"
```

The installable outputs are:

- `.Build\packages\test\DearModdingUI\`
- `.Build\packages\DearModdingUI-0.1.0-test.zip`
- `.Build\packages\test\DearModdingUI-MCM\`
- `.Build\packages\DearModdingUI-MCM-0.1.0-test.zip`

The core archive contains the host and general test client. Install the separate
MCM test archive as well when exercising the real bridge and its Scaleform probe.
Neither archive includes documentation or debug symbols.

The project removes CommonLib automatic install mappings. Build and package
write only below this checkout. PDBs remain beside `.Build\test` binaries and
are not included in the installable package.

## Manual setup and checklist

1. Install or enable the assembled core and MCM test packages in a disposable manual
   development profile. The test client performs its single host discovery/registration
   attempt at F4SE `kPostPostLoad`, after all plugins have loaded; it does not
   retry later or on Present.
2. Start the game manually. Open the host and select **DMUI Tests**, then use
   its **Exercises** pages. Verify host state is ready, API is 0.1, UI ABI is
   1, the negotiated UI revision is 1, and all required service bits are
   reported. If the host was absent or
   incompatible at `kPostPostLoad`, verify the test client logged once and
   registered nothing. Also inspect the three clearly labeled `[Fixture]`
   navigation, status, and in-memory configuration clients. MCM coverage in
   game comes from the packaged real MCM bridge, not a synthetic runtime copy.
3. With the host menu closed, use `Ctrl+Shift+F10` to toggle the overlay and
   `Ctrl+Shift+F11` to schedule the bounded delayed toast. Verify movement and
   gameplay input remain unaffected.
4. Exercise menu, console, text-entry, focus-loss, and other obstructed
   contexts. The gameplay defaults must yield. Bind the default-NONE
   `HOST_INPUT_INACTIVE`, optional `ALWAYS`, letter `A`, and digit `7` probes
   in the host hotkey manager; toggle their enable state on the exercise page and
   verify effective binding/conflict plus press/release counters.
5. Verify the overlay timer and observer samples continue while the overlay
   and host menu are hidden. Try all anchors, free X/Y, scale, opacity, and
   arrangement. Free movement must work only while the host menu owns input;
   verify placement generation and arrangement-completed count.
6. Verify both the host-owned CPU-pixel image and the existing imported
   checker/gradient appear. Use **Update CPU image** repeatedly; the same
   handle should alternate dimensions, colors, and alpha. Use **Cycle /
   recreate image**, then **Release after queued draw** for the imported SRV;
   the queued draw should survive release and one replacement should be
   imported. Renderer/device changes should invalidate and recreate both
   resources without growing slots.
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

The test client writes bounded, event-driven diagnostics to
`Documents\My Games\Fallout4\F4SE\dmui-test-client.log`. It records the
one-time initialization and service preflight, each hotkey's registration and
initial effective binding, hotkey edges, overlay frame-demand changes, image
lifecycle transitions and CPU create/update outcomes, notification
scheduling/posting, completed edits and resets, and dialog state transitions.
It never logs entered text and does not log ordinary per-frame queries or
draws.
The standalone preview sends the same fixture diagnostics to standard error.

The preview and F4SE fixture use the same public `dmui::ui` callbacks and
negotiate the same host UI table. Only the thin environment adapters differ.

Use **Log current results** on the results page for an on-demand compact
snapshot. Another compact snapshot is written whenever any exercise page is
left, including switches to another exercise and closing the menu. Snapshot
outcomes are labeled `observed`, `failed`, or `unexercised`; unexercised probes
remain unexercised, and a snapshot never treats the absence of errors as an
overall pass.

These checks validate the stable UI API harness. They do **not** validate a
consumer's proxy-swapchain attachment or renderer-replacement integration.

After the manual test, remove or disable the installed test packages. No cleanup
or deployment action is performed by the build.
