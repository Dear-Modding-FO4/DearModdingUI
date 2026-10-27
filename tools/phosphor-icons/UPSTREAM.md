# Phosphor icon vocabulary sources

The generated icon vocabulary combines the shipped webfont catalog with the
structured search metadata published by Phosphor Icons.

- Font catalog: `@phosphor-icons/web` 2.1.2, tag `v2.1.2`, commit
  `70854726d7bd82ae21f0dc81b5b5c35240a77066`
- Source selection manifest SHA-256:
  `4c2141ee6e8ed9b53466b26e483ca4d85fcf2044138029095d56f910e1b9dab2`
- Shipped Fill font SHA-256:
  `a53f5d2630cab5e3b7536ecb9d69d71519a2190298c22b1f8d770dd37bc2940a`
- Search metadata: `@phosphor-icons/core` 2.1.1
- Package integrity:
  `sha512-v4ARvrip4qBCImOE5rmPUylOEK4iiED9ZyKjcvzuezqMaiRASCHKcRIuvvxL/twvLpkfnEODCOJp5dM4eZilxQ==`
- Package archive SHA-256:
  `313332be6190b724da24107addd781799b48bf76b13963f24501112ffe1baadd`

`PhosphorFillCatalog.json` is the normalized name/codepoint export from the
2.1.2 Fill `selection.json`. `PhosphorMetadata.json` is the normalized
`icons` structured export from the core package. Categories are deliberately
not included in inference. `DomainIconPreferences.json` contains the small
reviewed Dear Modding semantic vocabulary.

The generator verifies the complete catalog and metadata snapshots using
SHA-256 of ASCII JSON with sorted keys and compact separators. This permits
line-ending changes without accepting modified upstream data. Updating either
snapshot requires explicitly updating its pinned digest in the generator.
Domain preferences are maintained locally and are not part of the upstream pin.

The core package reports nine codepoints that differ from the shipped webfont.
The generator records the exact reviewed pairs and rejects any change to that
set. Names, not core codepoints, join search tags to the authoritative webfont
glyphs; these known upstream discrepancies never replace a shipped codepoint.

Regenerate and verify the checked-in host header offline:

```powershell
python tools/phosphor-icons/GeneratePhosphorGlyphs.py
python tools/phosphor-icons/GeneratePhosphorGlyphs.py --check
```

When validating against the host's bundled font, also pass:

```powershell
python tools/phosphor-icons/GeneratePhosphorGlyphs.py --check --font <path-to-Phosphor-Fill.ttf>
```

To re-export the pinned snapshots, download the selection manifest from
`https://raw.githubusercontent.com/phosphor-icons/web/70854726d7bd82ae21f0dc81b5b5c35240a77066/src/fill/selection.json`
and the core archive from
`https://registry.npmjs.org/@phosphor-icons/core/-/core-2.1.1.tgz`.
Verify their SHA-256 values above before extracting or loading the package.
Keep these temporary inputs under `.Build`.

With the manifest at `.Build\selection.json` and the package extracted under
`.Build\package`, run this from the repository root. It preserves the
checked-in provenance and reads the package's structured export directly:

```powershell
@'
const fs = require("node:fs");
const path = require("node:path");
const { pathToFileURL } = require("node:url");
(async () => {
  const root = path.join("tools", "phosphor-icons");
  const catalogPath = path.join(root, "PhosphorFillCatalog.json");
  const metadataPath = path.join(root, "PhosphorMetadata.json");
  const catalog = JSON.parse(fs.readFileSync(catalogPath, "utf8"));
  const metadata = JSON.parse(fs.readFileSync(metadataPath, "utf8"));
  const selection = JSON.parse(fs.readFileSync(
    path.join(".Build", "selection.json"), "utf8"));
  const { icons } = await import(
    pathToFileURL(path.resolve(".Build", "package", "dist", "index.mjs")).href);
  catalog.glyphs = selection.icons.map(({ properties }) => {
    const [name, ...aliases] = properties.name.split(",")
      .map(value => value.trim().replace(/-fill$/, ""));
    return { name, aliases, codepoint: properties.code };
  }).sort((a, b) => a.name < b.name ? -1 : a.name > b.name ? 1 : 0);
  metadata.icons = icons.map(({ name, pascal_name, alias, tags, codepoint }) => ({
    name, pascal_name, ...(alias ? { alias } : {}), tags, codepoint
  }));
  fs.writeFileSync(catalogPath, JSON.stringify(catalog, null, 2) + "\n");
  fs.writeFileSync(metadataPath, JSON.stringify(metadata, null, 2) + "\n");
})().catch(error => { console.error(error); process.exitCode = 1; });
'@ | node
python tools/phosphor-icons/GeneratePhosphorGlyphs.py --check
```

The Phosphor inputs are licensed under the adjacent `LICENSE` file.
