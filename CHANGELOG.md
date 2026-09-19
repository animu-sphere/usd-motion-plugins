# Changelog

All notable changes to `usd-motion-plugins` are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html). The
motion contract, joint vocabulary and USD mapping carry their own versions,
separate from the package version
([docs/architecture/WORKSPACE.md §4](docs/architecture/WORKSPACE.md#4-versioning-and-build)).

## [Unreleased]

### Added

- **The documentation baseline.** The design policy, accepted on 2026-09-17
  with §42 recording the decisions taken while `usd-mmd-plugins` and
  `usd-vrm-plugins` aligned with it; three proposed contracts written from
  `usd-vrm-plugins`' measured implementation — `MOTION_CONTRACT.md`,
  `RETARGETING_POLICY.md`, `USD_MAPPING.md`; the workspace contract and
  external dependencies, with every identity reserved and its source named;
  the capability matrix and diagnostics catalog, both empty; the roadmap,
  mapping releases v0.1.0–v0.5.0 onto the imports and Migration Phase A–F.
  No code.
- **The scaffold, with no component in it.**
  - WS-O1 is decided: lower-camel identities that are also each library's
    directory, CMake package and exported target (`motionCore::motionCore`),
    and snake_case CLI commands. That is the siblings' discipline, recorded as
    design policy §42.7 and swept through every document.
  - The root project reads `VERSION` (0.1.0) and builds as C++20.
    `cmake/UsdMotionOpenUsd.cmake` refuses any OpenUSD but 26.08.
    `CMakePresets.json` covers plain CMake on all three platforms, and
    `openstrata.toml` and `openstrata.ci.yaml` carry the siblings' three
    runtime digests and `ost` 0.22.10.
  - `scripts/check_docs.py` checks links, anchors and every version and pin
    mirror, both in CTest and in the hand-written `docs-check` workflow.
  - `workspace_installed_consumer` installs the tree into a clean prefix,
    scans it for build paths, and builds a consumer copied outside the
    repository against every package in
    `tests/installed_consumer/packages.json`. The list is empty until the first
    import, and the lane already runs.
  - The community files, and a building guide.
- **`motionCore`, imported from `usd-vrm-plugins` with its history**
  (Migration Phase A–B). Its 23 commits arrived through `git filter-repo`,
  and the types were renamed on arrival in a commit of their own
  (DESIGN_POLICY.md §42.2): namespace `openstrata::motion`; `HumanJoint`,
  `MotionPose`, `MotionClip`, `SourceMetadata`, `MotionChannelSet` and
  `MotionChannel` (whose weight is its `value`); `Humanoid.h` is
  `MotionPose.h`. Four contract questions were decided before it landed:
  MC-O1 (the 55-joint vocabulary is version 1), USD-O1 (`Skeleton` / `Body` /
  `Channels`), USD-O2 (always 30 time codes per second) and MC-O4's scalar
  case (a channel's value is a `float`). The boundary check refuses product,
  device and avatar-format names in code. The package is `SameMinorVersion`,
  and it is the installed-consumer lane's first row.
- **The rendered CI workflow**, `ost-source-ci.yml`, with a graph cell ahead
  of the three workspace cells. It could not be rendered for an empty
  workspace under `ost` 0.22.10.
  - The rendered `ost` workflow is **not** included, because `ost` 0.22.10
    refuses a workspace graph with no member
    ([roadmap](docs/roadmap/current.md)).
