# Third-party notices

## Dear ImGui

Dear ImGui is copyright 2014-2026 Omar Cornut and is distributed under the MIT License. Its complete license is retained at `Depends/imgui/LICENSE.txt`.

## cimgui

cimgui is copyright 2015-2026 Stefano D'Ambro and is distributed under the MIT License. Its complete license is retained at `Depends/cimgui/LICENSE`.

## CommonLibF4 and toml11

The Dear Modding FO4 CommonLibF4 fork is distributed under GPL-3.0-or-later with the Modding Exception and GPL-3.0 Linking Exception (with Corresponding Source). See `Depends/commonlibf4/LICENSE` and `Depends/commonlibf4/EXCEPTIONS`.

toml11 is distributed under the MIT License, retained at `Depends/toml11/LICENSE`.

## JSON for Modern C++

JSON for Modern C++ is copyright 2013-2025 Niels Lohmann and is distributed under the MIT License. Version 3.12.0 is retained as the `Depends/nlohmann-json` submodule, including its complete license as `LICENSE.MIT`.

## Community Shaders UI work

The DearModdingUI shell, theme, font roles, cursor behavior, and blur were ported from Fallout 4 Community Shaders under GPL-3.0. The Gaussian blur credits Unrimp by Christian Ofenberg under the MIT License.

## Fonts

Jost and Atkinson Hyperlegible are distributed under the SIL Open Font License 1.1. Their license texts are retained beside the font files as `OFL.txt`; Atkinson Hyperlegible also includes an `UPSTREAM.md` provenance record.

The Phosphor icon font is distributed under the MIT License. Its license and pinned upstream provenance are retained beside the font as `LICENSE` and `UPSTREAM.md`.

## Optional semantic icon comparison

`tools\icon-comparison` acquires dependencies only for the opt-in offline experiment;
none are included in host or MCM release packages.

Open English WordNet 2025 is developed by the Open English WordNet team from
Princeton WordNet. Its CC BY 4.0 and Princeton WordNet terms require credit to both.
The comparison creates a modified local index and displays selected definitions
and relation paths. The pinned license sources are recorded in
`tools\icon-comparison\dependencies\Acquisition.json`.

BAAI's BGE-small-en-v1.5 model declares the MIT License. Its pinned model card,
tokenizer configuration, and FP32 ONNX weights are acquired together. ONNX Runtime
is MIT-licensed and Hugging Face Tokenizers is Apache-2.0-licensed.
The complete pinned wheel dependency and license inventory is recorded in
`tools\icon-comparison\dependencies\Dependencies.json`; original package notices
are retained in the acquired wheels/environment. Acquired data, weights, and
runtimes are not committed to this repository.
