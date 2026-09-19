#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Enforce motionSampling's boundary (docs/architecture/WORKSPACE.md §2.1-§2.4).

motionSampling sits one level above motionCore: it may depend on motionCore
and OpenUSD's Gf value types, and on nothing else. Four checks: no stage,
plugin, registration or OpenExec API in the sources; no include from a
repository library outside its declared edges, and no transport; a link line
and a binary that import nothing from OpenUSD beyond its foundation value
types; and no product, device or avatar-format name in the code or its string
literals. Comments may cite where a rule came from, so they are stripped before
scanning.
"""

from __future__ import annotations

import os
import pathlib
import re
import shutil
import subprocess
import sys

LIBRARY = "motionSampling"
# WORKSPACE.md §2.1: the repository libraries this one may include.
ALLOWED_LIBRARIES = {"motionCore", "motionSampling"}


def _find_dumpbin() -> str | None:
    tool = shutil.which("dumpbin")
    if tool:
        return tool
    roots = [
        pathlib.Path(os.environ.get("ProgramFiles", r"C:\\Program Files")),
        pathlib.Path(os.environ.get("ProgramFiles(x86)", r"C:\\Program Files (x86)")),
    ]
    for root in roots:
        # The release year is a wildcard rather than "2022", and the reason is a
        # measurement: this machine's VS 2022 was replaced by VS 18 in place on
        # 2026-08-25, leaving an empty `2022/` beside a populated `18/`. Every
        # boundary check in the tree then reported "dumpbin was not found" and
        # failed -- nine red names for an editor upgrade, none of them about a
        # boundary. A locator that names one release of a tool it only needs
        # `/dependents` from is a version pin with no reason to exist.
        matches = sorted(root.glob(
            "Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"),
            reverse=True)
        if matches:
            return str(matches[0])
    return None


def _strip_comments(text: str) -> str:
    """C++ text with // and /* */ comments blanked, string literals kept.

    Line structure is preserved so a finding keeps its line number.
    """
    out: list[str] = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _binary_dependencies(library: pathlib.Path) -> str:
    if sys.platform == "win32":
        tool = _find_dumpbin()
        if not tool:
            raise RuntimeError("dumpbin was not found")
        command = [tool, "/nologo", "/dependents", str(library)]
    elif sys.platform == "darwin":
        tool = shutil.which("otool")
        if not tool:
            raise RuntimeError("otool was not found")
        command = [tool, "-L", str(library)]
    else:
        tool = shutil.which("readelf")
        if not tool:
            raise RuntimeError("readelf was not found")
        command = [tool, "-d", str(library)]
    return subprocess.run(
        command, check=True, text=True, encoding="utf-8", errors="replace",
        stdout=subprocess.PIPE).stdout


def main() -> int:
    source = pathlib.Path(sys.argv[1]).resolve()
    library = pathlib.Path(sys.argv[2]).resolve()
    errors: list[str] = []

    forbidden_files = {"openstrata.plugin.yaml", "pluginfo.json"}
    for path in source.rglob("*"):
        if path.is_file() and path.name.lower() in forbidden_files:
            errors.append(f"plugin registration file is forbidden: {path}")

    # Gf value types are allowed; stage, composition, registration and OpenExec
    # APIs are not: motionUsd and the plugin bundles own those.
    forbidden_source = re.compile(
        r"pxr/(?:usd|base/(?:tf|plug)|imaging|exec)/|PXR_NAMESPACE|"
        r"TF_REGISTRY_FUNCTION|SDF_DEFINE_FILE_FORMAT|"
        r"\b(?:UsdStage|SdfLayer|PlugRegistry|EsfStage|VdfNode)\b",
        re.IGNORECASE)
    # No transport: a stream receives poses already decoded (MOTION_CONTRACT.md
    # §9), and a socket here would take the replayability with it.
    forbidden_transport = re.compile(
        r"\b(?:winsock2?|sys/socket\.h|asio|curl|websocket)\b", re.IGNORECASE)
    # Every include of a repository library names its include root; a root
    # outside ALLOWED_LIBRARIES is an edge WORKSPACE.md §2.1 does not draw.
    repository_include = re.compile(r'#\s*include\s*[<"](motion[A-Z]\w*)/')
    # Product, device and avatar-format names (WORKSPACE.md §5, invariant 3).
    product_names = re.compile(
        r"(?<![a-z0-9])(?:vrma?|vroid|mtoon|vmc|mocopi|vrchat|unity|mmd|pmx|vmd)"
        r"(?![a-z0-9])",
        re.IGNORECASE)
    for area in (source / "include", source / "src"):
        for path in sorted(area.rglob("*")):
            if not path.is_file():
                continue
            code = _strip_comments(path.read_text(encoding="utf-8"))
            if forbidden_source.search(code):
                errors.append(f"stage/plugin/exec API is forbidden: {path}")
            if forbidden_transport.search(code):
                errors.append(f"a transport is forbidden: {path}")
            for match in repository_include.finditer(code):
                if match.group(1) not in ALLOWED_LIBRARIES:
                    errors.append(f"{path}: includes {match.group(1)}, an edge "
                                  f"{LIBRARY} does not have")
            for number, line in enumerate(code.splitlines(), 1):
                match = product_names.search(line)
                if match:
                    errors.append(f"{path}:{number}: product name '{match.group(0)}' "
                                  f"in {LIBRARY}'s code")

    cmake = re.sub(r"#[^\n]*", "",
                   (source / "CMakeLists.txt").read_text(encoding="utf-8"))
    if re.search(r"(?:target_link_libraries\([^)]*(?:\busd\b|\bsdf\b|\bplug\b|"
                 r"\bar\b|exec)|pxr::(?:usd|sdf|plug|ar))", cmake, re.IGNORECASE):
        errors.append(f"{LIBRARY} CMake must link only its declared libraries and "
                      "the OpenUSD gf value library")

    try:
        dependencies = _binary_dependencies(library)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        errors.append(f"could not inspect {LIBRARY} dependencies: {exc}")
        dependencies = ""
    forbidden_binary = re.compile(
        r"(?:usd_ms|lib(?:usd|sdf|plug|ar)(?:[._-]|\.(?:dll|dylib|so)))",
        re.IGNORECASE)
    if forbidden_binary.search(dependencies):
        errors.append(f"{LIBRARY} binary imports an OpenUSD stage/plugin library")

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"{LIBRARY} boundary check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
