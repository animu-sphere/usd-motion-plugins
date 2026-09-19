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
- **The evidence `usd-vrm-plugins` handed over (its MIG-0)**, as proposed
  contract text that cites the measurement rather than restating it:
  - `EXEC_CONTRACT.md`, a fourth focused contract. It holds the OpenExec driver
    contract (ten rules and three codes), the one-joint fallback, and the
    producer conventions with a proposed author for each. Only
    `motion:timeCodesPerSecond` is a motion writer's to author, as a shim until
    OpenUSD delivers stage metadata to a computation. The filter,
    root-intake, placement, blend and retarget-policy attributes belong to the
    composed scene.
  - `RETARGETING_POLICY.md` gains the v0.9.0 partial-skeleton (§4.1) and
    scale (§6.1) decisions, which closes RT-O2 and RT-O3, and the six
    retarget-side findings from the OpenExec evidence (§10).
  - `MOTION_CONTRACT.md` gains the recorded-source provenance narrowing (§7.1)
    and the tracker boundary (§11.1); `USD_MAPPING.md` §4.1 gains the rate
    attribute.
- **`motionRuntime`, imported from `usd-vrm-plugins` with its history, as
  `motionSampling` and `motionRecording`** (Migration Phase C). Its 28 commits
  arrived through `git filter-repo`; a move-only commit then split its files
  between the two libraries WORKSPACE.md §1.1 reserves, and the rename followed
  in a commit of its own. `motionSampling` holds interpolation, resampling, the
  filter, blending, the pose buffer and the status-carrying sampling interface;
  `motionRecording` holds live intake, the `motion-capture-trace` format,
  replay, the recorder — renamed `MotionRecorder` — and the seven-trace corpus
  with its generator. The trace format is unchanged, and `.gitattributes` keeps
  traces LF so a Windows checkout still round-trips them byte for byte. Each
  library has its own package (`SameMinorVersion`), label and boundary check,
  which also refuses an include across an undeclared edge and any transport
  header.
- **The rendered CI workflow**, `ost-source-ci.yml`, with a graph cell ahead
  of the three workspace cells. It could not be rendered for an empty
  workspace under `ost` 0.22.10.
  - The rendered `ost` workflow is **not** included, because `ost` 0.22.10
    refuses a workspace graph with no member
    ([roadmap](docs/roadmap/current.md)).

### Changed

- **The four sampling findings from `usd-vrm-plugins`' OpenExec layer are
  fixed** (MOTION_CONTRACT.md §8). Each is a pure function, and the streaming
  class beside it calls it, so each rule has one implementation:
  - `SampleClip(clip, t)` answers a `PoseSampleResult` from a clip held by
    reference. Its ordering precondition is written on it. `ClipSource`,
    `SampleAnimation` and `PoseBuffer::Sample` now share one bracket-and-hold
    search.
  - `PoseFilter::Step(state, pose, options)` returns the state beside the pose,
    so a caller that holds the recurrence keeps a dropped joint's history.
    `Apply` is `Step` over the object's own state.
  - The N-way `BlendPoses` now returns `std::optional<MotionPose>`, nullopt
    when there is nothing to blend. A NaN weight counts as no weight. The
    result is stamped at the first weighted source's instant, where it used to
    interpolate the sources' timestamps. The order dependence is stated in the
    header.
  - `ConditionRootMotion(prior, pose, intake)` is the root intake rule that
    used to be private to `LiveCaptureSource`.
- **A pose's provenance is its non-optional `metadata`** (MOTION_CONTRACT.md
  §5.1, §7), where the imported pose carried an optional `source`. The default
  metadata is how a producer says it recorded nothing, so "unknown" has one
  spelling instead of two. `SourceMetadata` gains the sample's own
  `sourceTimestamp` and `sequenceNumber`, both optional and both part of
  `==` but not of `NearlyEqual`. The stream names the source and each sample
  keeps its own stamp and counter: `LiveCaptureSource` keeps the ones a
  connector pushed, interpolation takes the nearer observation's whole, and
  `MotionRecorder` leaves them off the clip.
- **`motion-capture-trace` is version 4**: a frame may carry `sequence` and
  `sourceTime` lines. Versions 1–3 still read; the writer refuses a
  non-finite stamp, as the reader does. The seven corpus traces changed in
  their version line only.
