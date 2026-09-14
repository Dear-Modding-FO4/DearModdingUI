# DearModdingUI test bundle

One archive contains the host, MCM bridge, native test client, and MCM fixture.
Run from the repository root:

```powershell
xmake f -P "$PWD" -m release --test-release=y -y
xmake package-release -P "$PWD"
```

Install `.Build\packages\DearModdingUI-<version>-test.zip`, enable
`DMUITests.esp`, and use a **new disposable save**. Normal host dependencies
apply; persisted MCM settings and hotkeys also require Mod Configuration Menu.

Use **DMUI Tests** for native exercises and **DMUI MCM Tests** for the real
bridge. Instructions are on the exercise pages. The preview uses the same MCM
configuration with simulated values; game actions require in-game testing.

Logs: `Documents\My Games\Fallout4\F4SE\dmui-test-client.log`.
Builds never install anything, and packages never include generated user MCM
settings or keybinds.

Fixture data: `tools\shared\fixtures\mcm\data`.
[Regenerate the checked-in ESP/PEX files](fixture-builder/README.md).
