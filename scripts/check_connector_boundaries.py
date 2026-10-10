#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check WORKSPACE §2.6 at source, manifest and configured-link boundaries.

The source lane needs only Python's standard library. --link-graph reads the
configured target properties emitted by UsdMotionBoundaryGraph.cmake, including
private, interface and imported dependencies. All property alternatives are
checked conservatively, including inactive generator-expression branches.
This is a structural check, not a C++ parser or a binary symbol audit. Existing
component gates retain their binary and format-specific checks.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[1]
LIBRARIES = {
    "motionCore", "motionSampling", "motionRecording", "motionRetarget",
    "motionUsd", "motionSource", "motionBvh",
}
# Match dependency names, paths and flags, never arbitrary provenance values.
# Boundaries around short names matter: 'osc' must not match 'Oscillation'.
DEPENDENCIES = re.compile(
    r"motion[-_]?connectors?|motionConnector\w*|usd[-_]?(?:avatar[-_]?runtime|vrm[-_]?plugins|mmd[-_]?plugins)|"
    r"(?<![a-z0-9])(?:lib|-l)?(?:openxr\w*|mediapipe\w*|oscpack|liblo|"
    r"osc|websocket\w*|ixwebsocket\w*|libwebsockets\w*|"
    r"asio|boost[/:]+(?:asio|beast)|curl|libcurl|winsock2?|ws2_32|wsock32|mswsock|winhttp|wininet|"
    r"sys/socket\.h|sys\/socket|netinet|arpa/inet\.h|netdb\.h|"
    r"emscripten|webxr|webgpu|napi|node_api\.h|javascriptcore|v8\.h|"
    r"xsens\w*|vmc\w*|mocopi\w*|kinect\w*|k4a|realsense\w*|librealsense\w*|"
    r"ultraleap\w*|leapc|leap\.h|natnet\w*|vicon\w*|qualisys\w*)"
    r"(?![a-z0-9])",
    re.IGNORECASE,
)
CONNECTOR_TYPES = re.compile(r"\b(?:MotionFrame|IMotionConnector|TrackerObservation)\b")
SOCKET_CALLS = re.compile(r"\b(?:socket|WSAStartup|WSASocket[AW]?|sendto|recvfrom|getaddrinfo)\s*\(")
PRODUCER = (r"(?:vmc|mocopi|vrchat|openxr|webxr|mediapipe|xsens|kinect|vicon|qualisys|"
            r"vrma?|vroid|mtoon|unity|mmd|pmx|vmd|rokoko|noitom|optitrack|"
            r"blender|maya|mixamo|motionbuilder|sony)")
PRODUCER_LITERAL = re.compile(r'"' + PRODUCER + r'"', re.IGNORECASE)
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', re.MULTILINE)
# String literals (ordinary, character and raw) and comments, in lexical order.
# Replacing with spaces preserves offsets/line numbers, including quoted URLs.
LEXEME = re.compile(
    r'R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"|'
    r'"(?:\\[\s\S]|[^"\\])*"|\'(?:\\[\s\S]|[^\'\\])*\'|'
    r'//[^\n]*|/\*[\s\S]*?\*/'
)


def blank(text: str) -> str:
    return re.sub(r"[^\n]", " ", text)


def code_text(text: str, *, strings: bool = False) -> str:
    # C++ line splicing precedes comment/identifier recognition.
    text = re.sub(r"\\\r?\n", "", text)
    return LEXEME.sub(
        lambda m: blank(m.group()) if strings or m.group().startswith(("//", "/*"))
        else m.group(), text,
    )


def product_check_text(text: str, component: str, where: str) -> tuple[str, list[str]]:
    """Legacy component product scans inspect code/includes, not provenance.

    Keep include names visible, blank other strings, and apply the shared
    dependency/type/source-comparison checks before the component's own rules.
    """
    comments_removed = code_text(text)
    code = code_text(text, strings=True)
    for match in INCLUDE.finditer(comments_removed):
        directive = match.start() + match.group().index("#")
        if code[directive] == "#":
            code = code[:match.start()] + match.group() + code[match.end():]
    return code, source_errors(text, component, where)


def source_errors(text: str, component: str, where: str) -> list[str]:
    comments_removed = code_text(text)
    code = code_text(text, strings=True)
    errors = []
    for match in INCLUDE.finditer(comments_removed):
        directive = match.start() + match.group().index("#")
        if code[directive] != "#":
            continue  # A preprocessor-looking line inside a raw string.
        header = match.group(1).replace("\\", "/")
        if DEPENDENCIES.search(header):
            errors.append(f"{where}: forbidden acquisition/transport include <{header}>")
        if component in LIBRARIES and header.startswith("pxr/exec/"):
            errors.append(f"{where}: OpenExec include in a motion library <{header}>")
        if component == "motionCore" and re.match(
            r"(?:motion(?:Sampling|Recording|Retarget|Source|Bvh|Usd)/|"
            r"pxr/(?:usd|exec|imaging|usdImaging)/|pxr/base/plug/)", header
        ):
            errors.append(f"{where}: dependency above motionCore <{header}>")
    for match in CONNECTOR_TYPES.finditer(code):
        errors.append(f"{where}: connector-owned type {match.group()}")
    for match in DEPENDENCIES.finditer(code):
        errors.append(f"{where}: acquisition/transport symbol {match.group()}")
    for match in SOCKET_CALLS.finditer(code):
        errors.append(f"{where}: network socket API {match.group().strip()}")
    # Reject direct source-name comparisons and lookup calls even across lines.
    # Assigning a literal to metadata is allowed. Indirection through variables
    # still requires code review; this scan does not perform data-flow analysis.
    for literal in LEXEME.finditer(comments_removed):
        if not PRODUCER_LITERAL.fullmatch(literal.group()):
            continue
        before = comments_removed[:literal.start()]
        after = comments_removed[literal.end():]
        if (re.search(r"(?:==|!=|\b(?:compare|strcmp|strcasecmp|find|contains)\s*\([^;{}]*)\s*$", before)
                or re.match(r"\s*(?:==|!=)", after)):
            errors.append(f"{where}: source-name processing comparison {literal.group()}")
    return errors


def manifest_errors(text: str, where: str) -> list[str]:
    # OpenStrata validates the full schema. Here, inspect only the dependency
    # block, so comments and descriptive/provenance fields are not dependencies.
    lines = text.splitlines()
    in_requires = False
    errors = []
    for number, line in enumerate(lines, 1):
        line = re.sub(r'"[^"\\]*(?:\\.[^"\\]*)*"|\'[^\']*\'|#[^\n]*',
                      lambda m: "" if m.group().startswith("#") else m.group(), line)
        if re.match(r"^requires\s*:", line):
            in_requires = True
        elif line.strip() and not line[0].isspace():
            in_requires = False
        if in_requires:
            match = DEPENDENCIES.search(line)
            if match:
                errors.append(f"{where}:{number}: forbidden manifest dependency {match.group()}")
    return errors


def repository_errors(root: pathlib.Path) -> list[str]:
    errors = []
    components = [path for area in ("libs", "tools", "plugins")
                  for path in sorted((root / area).glob("*")) if path.is_dir()]
    if not components:
        return [f"{root}: no components found; nothing was checked"]
    for component in components:
        sources = [path for area in ("include", "src")
                   for path in sorted((component / area).rglob("*"))
                   if path.suffix.lower() in {".h", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".inl"}]
        for path in sources:
            errors.extend(source_errors(path.read_text(encoding="utf-8"),
                                        component.name, str(path.relative_to(root))))
        manifests = sorted(component.glob("openstrata.*.yaml"))
        if not manifests:
            errors.append(f"{component.relative_to(root)}: no component manifest")
        for path in manifests:
            errors.extend(manifest_errors(path.read_text(encoding="utf-8"), str(path.relative_to(root))))
    return errors


def link_names(value: str) -> list[str]:
    """Target/library basenames, without genex operators or local directories."""
    names = []
    for token in re.split(r"[;$<>,\s]+", value):
        name = token.replace("\\", "/").rsplit("/", 1)[-1].rsplit(":", 1)[-1]
        name = re.sub(r"\.(?:lib|a|so(?:\.[0-9.]+)?|dylib|dll)$", "", name)
        name = re.sub(r"^(?:lib|-l)", "", name)
        name = re.sub(r"^usd_", "", name)
        names.append(name)
    return names


def openusd_arch_socket_dependency(target: str, node: dict, dependency: str) -> bool:
    # OpenUSD 26.08 arch links Ws2_32 on Windows. This one imported foundation
    # edge is inherited by gf/tf; it is not permission for motion to use sockets.
    return (dependency.lower() == "ws2_32" and target.rsplit("::", 1)[-1] == "arch"
            and node.get("imported") is True
            and any(re.search(r"[/\\](?:lib)?usd_arch\.(?:lib|dll)$", value, re.IGNORECASE)
                    for value in node["properties"]))


def graph_errors(graph: dict) -> list[str]:
    if graph.get("schema") != 1 or not graph.get("roots") or not graph.get("targets"):
        return ["link graph: missing schema, roots or targets"]
    nodes = graph["targets"]
    errors = []
    for root in graph["roots"]:
        root_library = root.rsplit("::", 1)[-1]
        pending = [(root, [root])]
        seen = set()
        while pending:
            target, trail = pending.pop()
            if target in seen:
                continue
            seen.add(target)
            if target not in nodes:
                errors.append(f"link graph: missing target {' -> '.join(trail)}")
                continue
            node = nodes[target]
            for value in [target, *node["properties"]]:
                for match in DEPENDENCIES.finditer(value):
                    if not openusd_arch_socket_dependency(target, node, match.group()):
                        errors.append(f"link graph: {' -> '.join(trail)}: forbidden dependency {match.group()}")
                names = link_names(value)
                if root_library in LIBRARIES and any(re.fullmatch(r"exec\w*|esf\w*|vdf", name) for name in names):
                    errors.append(f"link graph: {' -> '.join(trail)}: OpenExec dependency")
                if root_library == "motionCore" and any(re.fullmatch(
                    r"motion(?:Sampling|Recording|Retarget|Source|Bvh|Usd)|usd\w*|ms|sdf|plug|hd\w*", name
                ) for name in names):
                    errors.append(f"link graph: {' -> '.join(trail)}: dependency above motionCore")
            pending.extend((dependency, [*trail, dependency]) for dependency in node["edges"])
    return sorted(set(errors))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path, default=REPO)
    parser.add_argument("--link-graph", type=pathlib.Path)
    args = parser.parse_args()
    errors = repository_errors(args.root.resolve())
    if args.link_graph:
        try:
            errors.extend(graph_errors(json.loads(args.link_graph.read_text(encoding="utf-8"))))
        except (OSError, ValueError, KeyError, TypeError) as exc:
            errors.append(f"could not inspect configured link graph: {exc}")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Connector boundary check passed" + (" (including configured links)" if args.link_graph else " (source/manifest)"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
