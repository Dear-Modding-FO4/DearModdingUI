import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


SCALAR_TYPES = {
    "ImGuiCol": "int",
    "ImGuiComboFlags": "int",
    "ImGuiDataType": "int",
    "ImGuiHoveredFlags": "int",
    "ImGuiID": "unsigned int",
    "ImGuiInputTextFlags": "int",
    "ImGuiSelectableFlags": "int",
    "ImGuiSliderFlags": "int",
    "ImGuiTableColumnFlags": "int",
    "ImGuiTableFlags": "int",
    "ImGuiTableRowFlags": "int",
    "ImGuiTreeNodeFlags": "int",
    "ImU32": "unsigned int",
}
TYPE_ORDER = [
    "ImU32",
    "ImGuiID",
    "ImGuiCol",
    "ImGuiComboFlags",
    "ImGuiDataType",
    "ImGuiHoveredFlags",
    "ImGuiInputTextFlags",
    "ImGuiSelectableFlags",
    "ImGuiSliderFlags",
    "ImGuiTableFlags",
    "ImGuiTableColumnFlags",
    "ImGuiTableRowFlags",
    "ImGuiTreeNodeFlags",
]
VECTOR_PUBLIC_TYPES = {
    "const ImVec2": "const ImVec2&",
    "const ImVec4": "const ImVec4&",
}
RETURN_TYPES = {
    "ImVec2_c": "ImVec2",
    "ImVec4_c": "ImVec4",
    "const ImVec4_c*": "const ImVec4&",
}
EXPORT_PATTERN = re.compile(
    r"^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+([A-Za-z_][A-Za-z0-9_]*)"
)
RESOLVED_SYMBOL_PATTERN = re.compile(r'Resolve<Function>\("([^"]+)"\)')
SUPPORT_SYMBOLS = {"DMUI_GetImGuiVersionNum", "DMUI_GetStyleMetrics"}


class GenerationError(RuntimeError):
    pass


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise GenerationError(f"cannot read {path}: {error}") from error


def load_allowlist(path: Path) -> tuple[list[str], dict[str, str]]:
    data = load_json(path)
    if not isinstance(data, dict) or set(data) != {"functions", "additional", "excluded"}:
        raise GenerationError(
            "allowlist must contain exactly functions, additional, and excluded"
        )

    functions = data["functions"]
    additional = data["additional"]
    excluded = data["excluded"]
    if not isinstance(functions, list) or not isinstance(additional, list):
        raise GenerationError("allowlist functions and additional must be arrays")
    names = functions + additional
    if not names or not all(isinstance(name, str) and name for name in names):
        raise GenerationError("allowlist function names must be non-empty strings")
    if len(names) != len(set(names)):
        raise GenerationError("allowlist function names must be unique")
    if not isinstance(excluded, dict) or not all(
        isinstance(name, str)
        and name in functions
        and isinstance(reason, str)
        and reason
        for name, reason in excluded.items()
    ):
        raise GenerationError("excluded entries must name base functions and give reasons")
    return names, excluded


def atomic_write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=path.parent,
        text=True,
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
            output.write(text)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def normalized_type(type_name: str) -> str:
    result = re.sub(r"\bsize_t\b", "std::size_t", type_name)
    result = re.sub(r"\bva_list\b", "std::va_list", result)
    result = re.sub(r"\bImVec2_c\b", "ImVec2", result)
    result = re.sub(r"\bImVec4_c\b", "ImVec4", result)
    return result


def public_type(type_name: str) -> str:
    normalized = normalized_type(type_name)
    return VECTOR_PUBLIC_TYPES.get(normalized, normalized)


def declare_argument(type_name: str, name: str, default: str | None = None) -> str:
    if type_name == "...":
        return "..."
    if "(*)" in type_name:
        declaration = type_name.replace("(*)", f"(*{name})", 1)
    else:
        declaration = f"{type_name} {name}"
    if default is not None:
        declaration += f" = {normalize_default(default)}"
    return declaration


def normalize_default(value: str) -> str:
    return re.sub(r"\bNULL\b", "nullptr", value)


def record_arguments(
    record: dict[str, Any],
    *,
    public: bool,
    include_varargs: bool = True,
) -> list[str]:
    defaults = record.get("defaults", {})
    arguments = []
    for argument in record["argsT"]:
        name = argument["name"]
        type_name = argument["type"]
        if type_name == "..." and not include_varargs:
            continue
        rendered_type = public_type(type_name) if public else normalized_type(type_name)
        default = defaults.get(name) if public else None
        arguments.append(declare_argument(rendered_type, name, default))
    return arguments


def argument_types(record: dict[str, Any]) -> list[str]:
    return [
        normalized_type(argument["type"])
        for argument in record["argsT"]
        if argument["type"] != "..."
    ]


def call_arguments(record: dict[str, Any]) -> list[str]:
    return [
        argument["name"]
        for argument in record["argsT"]
        if argument["type"] != "..."
    ]


def safe_default(type_name: str) -> str:
    if type_name == "bool":
        return "false"
    if type_name in {"float", "double"}:
        return "0.0f" if type_name == "float" else "0.0"
    if type_name in {"ImVec2", "ImVec4"}:
        return "{}"
    if type_name.endswith("*"):
        return "nullptr"
    if type_name in {"int", "unsigned int", "ImGuiID", "ImU32"}:
        return "0"
    raise GenerationError(f"no safe default is defined for return type {type_name}")


def render_regular_wrapper(name: str, record: dict[str, Any]) -> list[str]:
    symbol = record["ov_cimguiname"]
    c_return = normalized_type(record["ret"])
    return_type = RETURN_TYPES.get(record["ret"], c_return)
    parameters = ", ".join(record_arguments(record, public=True))
    function_types = ", ".join(argument_types(record))
    function_types = function_types or "void"
    calls = ", ".join(call_arguments(record))
    nodiscard = "[[nodiscard]] " if return_type != "void" else ""
    lines = [
        f"\t{nodiscard}inline {return_type} {name}({parameters}) noexcept",
        "\t{",
        f"\t\tusing Function = {c_return} (*)({function_types});",
        f'\t\tstatic const Function function = detail::Resolve<Function>("{symbol}");',
    ]

    if record["ret"] == "const ImVec4_c*":
        lines.extend(
            [
                "\t\tstatic const ImVec4 fallback{};",
                "\t\tif (!function)",
                "\t\t\treturn fallback;",
                f"\t\tconst auto result = function({calls});",
                "\t\treturn result ? *result : fallback;",
            ]
        )
    elif return_type == "void":
        lines.extend(
            [
                "\t\tif (function)",
                f"\t\t\tfunction({calls});",
            ]
        )
    else:
        lines.extend(
            [
                "\t\tif (!function)",
                f"\t\t\treturn {safe_default(return_type)};",
                f"\t\treturn function({calls});",
            ]
        )
    lines.extend(["\t}", ""])
    return lines


def render_varargs_wrapper(
    name: str,
    record: dict[str, Any],
    twin: dict[str, Any],
) -> list[str]:
    if record.get("ret", "void") != "void" or twin.get("ret", "void") != "void":
        raise GenerationError(f"{name}: non-void varargs wrappers are unsupported")
    named_arguments = record_arguments(record, public=True, include_varargs=False)
    parameters = ", ".join([*named_arguments, "..."])
    function_types = ", ".join(argument_types(twin))
    twin_calls = call_arguments(twin)
    if not twin_calls or twin_calls[-1] != "args":
        raise GenerationError(f"{name}: va_list twin must end in an args parameter")
    named_calls = [argument["name"] for argument in record["argsT"] if argument["type"] != "..."]
    if not named_calls or named_calls[-1] != "fmt":
        raise GenerationError(f"{name}: fmt must be the last named varargs parameter")
    calls = ", ".join([*named_calls, "args"])
    symbol = twin["ov_cimguiname"]
    return [
        f"\tinline void {name}({parameters}) noexcept",
        "\t{",
        f"\t\tusing Function = void (*)({function_types});",
        f'\t\tstatic const Function function = detail::Resolve<Function>("{symbol}");',
        "\t\tif (!function)",
        "\t\t\treturn;",
        "\t\tstd::va_list args;",
        "\t\tva_start(args, fmt);",
        f"\t\tfunction({calls});",
        "\t\tva_end(args);",
        "\t}",
        "",
    ]


def collect_records(
    definitions: dict[str, Any],
    names: list[str],
    excluded: dict[str, str],
) -> tuple[list[tuple[str, dict[str, Any], dict[str, Any] | None]], set[str]]:
    wrappers = []
    symbols = set()
    for name in names:
        if name in excluded:
            continue
        key = f"ig{name}"
        records = definitions.get(key)
        if not isinstance(records, list) or not records:
            raise GenerationError(f"allowlisted function is absent from definitions: {key}")
        for record in records:
            if record.get("stname") or record.get("constructor") or record.get("templated"):
                raise GenerationError(f"{key}: only non-templated namespace functions are supported")
            twin = None
            if record.get("isvararg"):
                twin_records = definitions.get(f"{key}V")
                if not isinstance(twin_records, list) or len(twin_records) != 1:
                    raise GenerationError(f"{key}: expected exactly one va_list twin")
                twin = twin_records[0]
                symbols.add(twin["ov_cimguiname"])
            else:
                symbols.add(record["ov_cimguiname"])
            wrappers.append((name, record, twin))
    return wrappers, symbols


def required_scalar_types(
    wrappers: list[tuple[str, dict[str, Any], dict[str, Any] | None]],
) -> set[str]:
    found = set()
    for _, record, twin in wrappers:
        records = [record] if twin is None else [record, twin]
        for candidate in records:
            type_text = " ".join(
                [candidate.get("ret", "void")]
                + [argument["type"] for argument in candidate["argsT"]]
            )
            for name in SCALAR_TYPES:
                if re.search(rf"\b{re.escape(name)}\b", type_text):
                    found.add(name)
    return found


def render_header(
    wrappers: list[tuple[str, dict[str, Any], dict[str, Any] | None]],
    enums: dict[str, Any],
    imgui_version_num: int,
) -> str:
    scalar_types = required_scalar_types(wrappers)
    lines = [
        "#pragma once",
        "",
        "// Generated by Tools/generate_imgui_forward.py; do not edit.",
        "// Call ImGui::IsForwardVersionCompatible() before drawing to detect ABI drift.",
        "// cimgui API definitions are distributed under the MIT License.",
        "",
        "#if !defined(NOMINMAX)",
        "#define NOMINMAX",
        "#endif",
        "",
        "#if !defined(WIN32_LEAN_AND_MEAN)",
        "#define WIN32_LEAN_AND_MEAN",
        "#endif",
        "",
        "#include <Windows.h>",
        "",
        "#include <DearModdingUI/API.h>",
        "",
        "#undef ERROR",
        "#undef MEM_RELEASE",
        "#undef MAX_PATH",
        "#undef PAGE_EXECUTE_READWRITE",
        "#undef IMAGE_DOS_SIGNATURE",
        "",
        "#include <cfloat>",
        "#include <cstdarg>",
        "#include <cstddef>",
        "",
        "typedef struct ImVec2_c",
        "{",
        "\tfloat x;",
        "\tfloat y;",
        "} ImVec2_c;",
        "",
        "typedef struct ImVec4_c",
        "{",
        "\tfloat x;",
        "\tfloat y;",
        "\tfloat z;",
        "\tfloat w;",
        "} ImVec4_c;",
        "",
        "using ImVec2 = ImVec2_c;",
        "using ImVec4 = ImVec4_c;",
        "",
    ]
    for name in TYPE_ORDER:
        if name in scalar_types:
            lines.append(f"using {name} = {SCALAR_TYPES[name]};")
    lines.extend(
        [
            "",
            "struct ImGuiInputTextCallbackData;",
            "using ImGuiInputTextCallback = int (*)(ImGuiInputTextCallbackData* data);",
            "",
        ]
    )

    for name in TYPE_ORDER:
        enum_values = enums.get(f"{name}_")
        if name not in scalar_types or not enum_values:
            continue
        lines.append(f"enum {name}_ : {name}")
        lines.append("{")
        for index, value in enumerate(enum_values):
            comma = "," if index + 1 < len(enum_values) else ""
            lines.append(f"\t{value['name']} = {value['value']}{comma}")
        lines.extend(["};", ""])

    lines.extend(
        [
            "namespace ImGui",
            "{",
            f"\tinline constexpr uint32_t kForwardImGuiVersionNum{{ {imgui_version_num}u }};",
            "",
            "\tnamespace detail",
            "\t{",
            "\t\tinline HMODULE HostModule() noexcept",
            "\t\t{",
            '\t\t\tstatic const HMODULE module = GetModuleHandleW(L"DearModdingUI.dll");',
            "\t\t\treturn module;",
            "\t\t}",
            "",
            "\t\ttemplate <class Function>",
            "\t\tFunction Resolve(const char* symbol) noexcept",
            "\t\t{",
            "\t\t\tconst auto module = HostModule();",
            "\t\t\treturn module ? reinterpret_cast<Function>(GetProcAddress(module, symbol)) : nullptr;",
            "\t\t}",
            "\t}",
            "",
            "\t[[nodiscard]] inline uint32_t GetHostImGuiVersionNum() noexcept",
            "\t{",
            "\t\tusing Function = uint32_t (DMUI_CALL*)(void) noexcept;",
            "\t\tstatic const Function function = detail::Resolve<Function>(\"DMUI_GetImGuiVersionNum\");",
            "\t\treturn function ? function() : 0u;",
            "\t}",
            "",
            "\t[[nodiscard]] inline bool IsForwardVersionCompatible() noexcept",
            "\t{",
            "\t\treturn GetHostImGuiVersionNum() == kForwardImGuiVersionNum;",
            "\t}",
            "",
            "\t[[nodiscard]] inline DMUI_Result GetStyleMetrics(DMUI_StyleMetrics& metrics) noexcept",
            "\t{",
            "\t\tusing Function = DMUI_Result (DMUI_CALL*)(DMUI_StyleMetrics*) noexcept;",
            "\t\tstatic const Function function = detail::Resolve<Function>(\"DMUI_GetStyleMetrics\");",
            "\t\tmetrics = {};",
            "\t\tmetrics.structSize = sizeof(metrics);",
            "\t\treturn function ? function(&metrics) : DMUI_RESULT_UNSUPPORTED_ABI;",
            "\t}",
            "",
        ]
    )
    for name, record, twin in wrappers:
        if twin is None:
            lines.extend(render_regular_wrapper(name, record))
        else:
            lines.extend(render_varargs_wrapper(name, record, twin))
    if lines[-1] == "":
        lines.pop()
    lines.extend(["}", ""])
    return "\n".join(lines)


def dumpbin_exports(dumpbin: Path, dll: Path) -> set[str]:
    if not dumpbin.is_file():
        raise GenerationError(f"dumpbin executable does not exist: {dumpbin}")
    if not dll.is_file():
        raise GenerationError(f"DLL does not exist: {dll}")
    result = subprocess.run(
        [str(dumpbin), "/nologo", "/exports", str(dll)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != 0:
        raise GenerationError(
            f"dumpbin failed with exit code {result.returncode}\n{result.stdout.rstrip()}"
        )
    return {
        match.group(1)
        for line in result.stdout.splitlines()
        if (match := EXPORT_PATTERN.match(line))
    }


def resolve_dumpbin(argument: Path | None) -> Path:
    if argument is not None:
        return argument.resolve()
    discovered = shutil.which("dumpbin")
    if discovered:
        return Path(discovered)
    raise GenerationError("--dumpbin is required when dumpbin is not on PATH")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate the DearModdingUI client-side ImGui forwarding header."
    )
    parser.add_argument("--definitions", required=True, type=Path)
    parser.add_argument("--allowlist", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--dll", type=Path)
    parser.add_argument("--dumpbin", type=Path)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    definitions_path = arguments.definitions.resolve()
    enum_path = definitions_path.with_name("structs_and_enums.json")
    definitions = load_json(definitions_path)
    structure_data = load_json(enum_path)
    constants = load_json(definitions_path.with_name("constants.json"))
    if not isinstance(definitions, dict):
        raise GenerationError("definitions root must be an object")
    if not isinstance(structure_data, dict) or not isinstance(
        structure_data.get("enums"), dict
    ):
        raise GenerationError("structs_and_enums.json does not contain an enums object")
    try:
        imgui_version_num = int(constants["IMGUI_VERSION_NUM"])
    except (KeyError, TypeError, ValueError) as error:
        raise GenerationError("constants.json has no valid IMGUI_VERSION_NUM") from error

    names, excluded = load_allowlist(arguments.allowlist.resolve())
    wrappers, symbols = collect_records(definitions, names, excluded)
    header = render_header(wrappers, structure_data["enums"], imgui_version_num)
    rendered_symbols = set(RESOLVED_SYMBOL_PATTERN.findall(header))
    expected_symbols = symbols | SUPPORT_SYMBOLS
    if rendered_symbols != expected_symbols:
        raise GenerationError("generated symbol references differ from the selected records")

    if arguments.dll is not None:
        exports = dumpbin_exports(
            resolve_dumpbin(arguments.dumpbin),
            arguments.dll.resolve(),
        )
        missing = sorted(expected_symbols - exports)
        if missing:
            raise GenerationError(
                f"{len(missing)} referenced symbols are not exported: {', '.join(missing)}"
            )
        imgui_exports = {name for name in exports if name.startswith("ig")}
        print(
            f"Validated {len(symbols)} referenced symbols against "
            f"{len(imgui_exports)} ig* DLL exports."
        )
    elif arguments.dumpbin is not None:
        raise GenerationError("--dumpbin requires --dll")

    atomic_write(arguments.output.resolve(), header)
    for name, reason in excluded.items():
        print(f"Excluded {name}: {reason}")
    print(
        f"Wrote {arguments.output.resolve()} "
        f"({len(header.encode('utf-8'))} bytes, {len(header.splitlines())} lines, "
        f"{len(wrappers)} thunks)."
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except GenerationError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
