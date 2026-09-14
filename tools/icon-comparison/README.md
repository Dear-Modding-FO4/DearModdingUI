# Semantic icon comparison

Opt-in, offline comparison of the pinned host resolver, Open English WordNet 2025,
and BGE-small-en-v1.5 FP32 ONNX. Nothing here changes the host, client API,
generated icon vocabulary, or packaged plugins.

## Setup

Run from this isolated worktree, not the primary checkout. Requires the existing
Windows x64 MSVC/xmake setup and standard CPython 3.14. Initialize required pinned
repository submodules if the build reports missing prerequisites.

```powershell
$env:XMAKE_GLOBALDIR = "$PWD\.Build\IconComparison\BuildCache\Global"
$env:XMAKE_PKG_INSTALLDIR = "$PWD\.Build\IconComparison\BuildCache\Packages"
$env:XMAKE_PKG_CACHEDIR = "$PWD\.Build\IconComparison\C"
$env:PYTHONDONTWRITEBYTECODE = '1'
xmake build -P "$PWD" -y dmui-icon-comparison
.\.Build\IconComparison\Bin\dmui-icon-comparison.exe --self-test
python tools\icon-comparison\RunComparison.py prepare
python tools\icon-comparison\Acquire.py validate
```

Acquisition is separate and explicit. The manifests pin data, weights, supporting
files, and the complete supported wheel closure. Inspect the manifest and obtain
download approval before running these commands on a new machine or after cleanup:

```powershell
python tools\icon-comparison\Acquire.py download
python tools\icon-comparison\Acquire.py prepare
python tools\icon-comparison\Acquire.py status
```

This spike's approved limits are 250,000,000 download bytes and 1 GiB of experiment
artifacts. Required repository build prerequisites are separate. Files remain in
ignored `.Build\IconComparison` directories; no global Python packages, Hub cache,
model runtime, or installed mods are changed. Normal inference never downloads.

## Compare

```powershell
$python = '.\.Build\IconComparison\Environment\Scripts\python.exe'
& $python tools\icon-comparison\RunComparison.py evaluate --split development
& $python tools\icon-comparison\RunComparison.py tune
& $python tools\icon-comparison\RunComparison.py evaluate --split held_out
& $python tools\icon-comparison\RunComparison.py evaluate --split all
& $python tools\icon-comparison\RunComparison.py benchmark
& $python tools\icon-comparison\RunComparison.py query --heading 'Status' --context 'Plugin health'
```

Open `.Build\IconComparison\Reports\Comparison.html`. It uses the repository's
shipped Phosphor font and needs neither a server nor network access. JSON reports
retain raw candidates and measurements. The `query` command also accepts
`--explicit-name` and `--explicit-glyph` (decimal or `0x` hexadecimal).

`Corpus.json` contains 60 English heading families and 95 query variants, split
30/30 by family. Source headings and synthetic cases are distinguished. The
initial acceptable sets are provisional; multiple icons and abstention may be
reasonable. Counts against those sets are triage, not confirmed accuracy.
Kuz must judge disputed choices before conclusions are treated as quality evidence.

`prepare` freezes corpus/catalog hashes. Threshold selection uses only development
results and the policy declared in `Evaluation.json`; `tune` is forbidden once
held-out access begins, including a failed run. Do not edit the corpus, templates, or algorithms in
response to held-out results and quietly rerun the same evaluation.

Kuz was unavailable to adjudicate development shortlist gaps. Abstention is
therefore calibrated using declared negative controls, maximizing issued
nonnegative candidates rather than pretending unreviewed positive sets establish
accuracy. The operating-point record retains the full development sweep.
Positive quality and disputed metaphors remain explicitly ungraded.

## Interpretation

The native helper calls the actual pinned `IconResolver` and exports its generated
name/alias/domain/tag arrays. Every existing selection is preserved, including
explicit Question and invalid raw glyph values. Experiments apply only on genuine
misses. Raw diagnostics on protected automatic matches do not change the result.

WordNet uses dataset synonyms and a bounded concept relation, with auditable
sense/path evidence. BGE uses a fixed template, CLS pooling, normalized
384-dimensional vectors, and cosine/margin abstention. Lexical tiers and cosine
scores are different quantities, neither a calibrated probability.

Benchmarks isolate each backend in fresh processes. They distinguish native
baseline work from bridge overhead, preparation from cached-index initialization,
and uncached batch-one inference from hypothetical cached lookups. Fresh-process
initialization is not OS-cache-cold. Python/ONNX process memory and helper binary
size are not production C++ host costs. ORT intra-op/inter-op threads are both one
and tokenizer parallelism is disabled; NumPy BLAS threading is not separately pinned.

The runtime uses local files with Hub offline/telemetry settings and a Python
socket audit guard. This verifies the Python inference path, not an OS-level
network sandbox. No model is loaded into Fallout 4. Native ONNX DLL feasibility
does not establish tokenizer parity, lifecycle behavior, or game compatibility.

## Checks and cleanup

```powershell
& $python -m unittest discover -s tools\icon-comparison\tests -p '*Tests.py'
xmake build -P "$PWD" -y dmui-tests
.\.Build\Tests\dmui-tests.exe
xmake verify-no-auto-install -P "$PWD"
```

After reviewing and preserving the report, cleanup is required:

```powershell
python tools\icon-comparison\Cleanup.py
python tools\icon-comparison\Cleanup.py --execute
```

Cleanup removes only named spike-owned data, weights, indexes, environment,
download/cache directories, helper binaries/objects, and bytecode. Source,
acquisition pins, frozen protocol records, and the small report remain.
The report stays viewable; rerunning inference requires explicit reacquisition.
`python tools\icon-comparison\RunComparison.py report` regenerates its HTML from
the retained results without loading either experimental backend.

Licenses and attribution are recorded in the acquisition manifests and acquired
notices. These dependencies belong only to the comparison, not release packages.
