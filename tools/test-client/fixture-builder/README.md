# MCM fixture regeneration

Prerequisites:

- Windows x64 with PowerShell
- Internet access for pinned tool downloads
- This repository checked out with the canonical MCM fixture config present

From the repository root, run:

```powershell
& tools\test-client\fixture-builder\Regenerate.ps1
```

The script stores tools and intermediates under `.Build\fixture-tools`, then
regenerates and validates the checked-in ESP and PEX files against the canonical
`config.json` and `keybinds.json`.

Pinned tools are Caprica `0.3.0`, Champollion `1.3.2`, .NET SDK `10.0.401`,
and Mutagen `0.54.4`. Exact URLs and hashes are recorded in
`Dependencies.psd1` and `packages.lock.json`.
