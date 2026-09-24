# Changelog

All notable changes to `usd-motion-plugins` are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html). The
motion contract, joint vocabulary and USD mapping carry their own versions,
separate from the package version
([docs/architecture/WORKSPACE.md §4](docs/architecture/WORKSPACE.md#4-versioning-and-build)).

## [Unreleased]

### Changed

- **`ost` is pinned to 0.23.6**, in `openstrata.ci.yaml`, the rendered source
  CI and the release lane. 0.23.5 and 0.23.6 stage an installable bundle and a
  root-built tool per target and package from that stage, which is what moving
  `execMotion`'s and the CLIs' outputs out of the source tree needs. Nothing
  moves yet: without install rules a bundle keeps its source-tree outputs.
- **The CMake build shares its infrastructure through `cmake/`.** The root
  and every library, tool and bundle include `cmake/UsdMotionProject.cmake`,
  which reads `VERSION`, applies the C++20 and build-type policy, resolves
  OpenUSD under the 26.08 pin, reaches sibling packages, installs a library as
  its `<P>::<P>` package and supplies the test plumbing. The eleven hard-coded
  standalone fallback versions are gone; every project reads `VERSION`, and
  `check_docs.py` fails one that restates a number. OpenUSD's targets are
  linked as `usdmotion::pxr::<name>` whichever spelling the install exports.
  Package names, targets and every exported CMake file are unchanged, except
  as below.
- **`motionSource` and `motionBvh` install `SameMinorVersion` package
  versions**, like the other five libraries: while a package is 0.x, a
  consumer asking for 0.5 is not handed 0.6
  ([DEPENDENCIES.md §2](docs/architecture/DEPENDENCIES.md#2-toolchain)).

### Fixed

- `motionBvhConfig.cmake` guarded its `find_dependency(motionSource)` on a C++
  namespace rather than the `motionSource::motionSource` target, so the guard
  never held.
- `execMotion`'s boundary test looked for `USDVRM_TEST_PYTHON`, a name from
  `usd-vrm-plugins` that nothing here sets; it uses `USDMOTION_TEST_PYTHON`.

## [0.5.1] - 2026-09-24

A publication release. It changes no library, bundle or tool behaviour. What
it adds is that the `execMotion` bundle and the three CLIs are in the registry
as well as the libraries, so another repository can pin them.

### Added

- **`execMotion` and the three CLIs are published as digest-pinned
  artifacts.** Through v0.5.0 they were attached to the GitHub release and not
  pushed, because no consumer could pin a bundle or a tool from another
  repository. `ost` 0.23.4 can, with `requires.bundles[].artifact` and
  `requires.tools[]`. `usd-vrm-plugins` needs both: its `execVrm` reads
  `execMotion`'s nodes by name, and it must re-run its parity rows against the
  consumed bundle before deleting its own copy. The release workflow now stages,
  imports and pushes every packaged bundle and tool the way it does a library,
  to the same OCI repository and under the same tag shape
  (`<member>-<version>-<target>`). The pin table (`external-library-pins.md` /
  `.json`) gains a `bundles` and a `tools` section. Each section's member count
  comes from the tree, as the library count already did, so a member that stops
  being packaged fails the publish job instead of going missing from the table.

- **`motion_convert_clip` and `motion_bvh_inspect_report` hand their tools
  paths no ANSI code page can spell.** A BVH and a profile named by path under
  `ユニコード-é/` are converted, and the clip is held to the same conversion
  from an ASCII directory, with the BVH's own non-ASCII name as its
  `sourceId`. The inspector's report over the same kind of path is held to an
  ASCII one. Built without the UTF-8 code-page manifest, both Windows
  executables read `é` as `e`. The converter then cannot find the profile and
  the inspector cannot open the file, which was measured. These claims were
  the `motion_bvh_convert` and `motion_bvh_inspect` legs of `usd-vrm-plugins`'
  `workspace_unicode_paths`. They land here first so that repository can
  delete its BVH tools without leaving the claims nowhere.

- **`motion_record_replay` hands the tool paths no ANSI code page can spell.**
  A trace under `ユニコード-é/歩き-é.trace` is replayed and held to the same
  replay from an ASCII directory. Without the UTF-8 code-page manifest the
  Windows executable reads `é` as `e` and cannot open the trace; measured by
  building it without the manifest. The claim was a leg of `usd-vrm-plugins`'
  `workspace_unicode_paths` while the tool was `motion_capture` there, and it
  comes here first so that repository can delete its copy without leaving it
  nowhere.

### Changed

- **The `ost` pin is 0.23.4.** Taken with the ecosystem. 0.23.4 lets a
  consumer pin a published bundle (`requires.bundles[].artifact`) and a
  published tool for its tests (`requires.tools[]`). Those are the two edges
  `usd-vrm-plugins` needs before it can delete its copy of `execMotion`
  (its ost report 45). It also discards a member build tree configured
  against another runtime. The re-render changes nothing but the version.

- **The `ost` pin was 0.23.3.** Taken with the ecosystem, for the consumers
  of what v0.5.0 published: 0.23.3 pulls, graphs and validates an external
  library artifact only a tool declares, which is how `usd-vrm-plugins`'
  `motion_retarget` consumes `motionUsd`, and it discards a build tree whose
  cache was configured against another runtime (`usd-vrm-plugins`' ost report
  44). Nothing here consumes an external library, so nothing here changes.

- **The `ost` pin was 0.23.2.** Taken for the consumers rather than for this
  repository: 0.23.2 makes the root `ost build` compose the external library
  artifacts a workspace's members declare, which is how every consumer of the
  packages v0.5.0 published builds against them
  (`usd-vrm-plugins`' ost report 43). Nothing here changes — this workspace
  consumes no external library — and the pin moves with the ecosystem.

## [0.5.0] - 2026-09-20

The first release. It carries the whole scope the roadmap numbered v0.1.0
through v0.5.0, because every identity those five releases were to deliver
arrived between 2026-09-19 and 2026-09-20 — faster than the releases could be
cut — and `usd-vrm-plugins` cannot delete its second copy of any of them until
all of them are installable. One tag rather than five, and the roadmap's
scope table records which row carried what.

### Added

- **A release lane, and with it the thing this repository exists for: published
  library artifacts.** `.github/workflows/release.yml` is hand-authored — the
  CI contract cannot express packaging every member or pushing to a registry —
  and a tag `vX.Y.Z` now builds the workspace on the same three OS cells and
  digest-pinned runtimes as the PR lane, runs the whole CTest suite and the
  bundle's pyramid, packages every member, and publishes **each library** as a
  digest-pinned OpenStrata artifact.
  - One OCI repository, `ghcr.io/animu-sphere/usd-motion-plugins`, tagged
    `<library>-<version>-<target>`. One repository rather than seven because a
    new GHCR package is private and is made public by hand, once per package.
  - The release's real output is a **pin table**, generated from what was
    actually pushed: per library, per target, the archive digest a consumer
    pins and the `oci://…@sha256:…` source it pulls from. It ships as
    `external-library-pins.{json,md}` and inside the release notes, ready to
    paste into a consumer's `requires.libraries`.
  - `execMotion` and the three CLIs are packaged and attached to the release
    but not pushed: nothing pins them by digest yet.
  - `workflow_dispatch` is a dry run — same lanes, same table with local
    digests and no `source`, no registry write, no release object.
  - `scripts/make_release_notes.py` and
    `docs/contributing/RELEASE_NOTES_TEMPLATE.md` render the notes from the
    changelog, and refuse a section still headed *unreleased* unless the run is
    a dry one.
  - `scripts/check_docs.py` gained the check that makes the hand-authored lane
    safe: its three `ost` pin sites must agree with `openstrata.ci.yaml`.
    usd-vrm-plugins let exactly that drift across three releases (its ost
    report 39) before adding the same check. Mutation-checked here.

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

- **`motionUsd`, authoring the motion stage** (USD_MAPPING.md §2–§5). The
  writer arrived from `usd-vrm-plugins`' `motion_capture` with its history,
  in a move of its own. It was then adapted to the mapping:
  - `/Animation`, `/Animation/Skeleton` and `/Animation/Body`
  - always 30 time codes per second, with times snapped to whole frames
  - identity `scales`, and `motion:timeCodesPerSecond` on `Body`
  - `customData.motion`

  `AuthorMotionStage` fills a stage a caller holds, and `WriteMotionStage`
  writes a file. `motionCore` gains `HumanJointVocabularyVersion`. The package
  is `SameMinorVersion`, and it is the installed-consumer lane's fourth row.

- **`motionUsd`'s reading half** (USD_MAPPING.md §7), imported from
  `usd-vrm-plugins`' `motion_retarget` with its history. 13 commits came
  through `git filter-repo`; a move-only commit and the adaptation followed.
  Only the clip and skeleton reading arrived — the avatar reading, the
  expressions, the look-at and the bake onto a VRM rig are that repository's.
  - `ReadMotionStage` and `OpenMotionStage` answer a `MotionClip`, the
    skeleton's `jointTokens` and `restTransforms`, the §5 metadata and a list
    of warnings. A skeleton whose tokens are not the vocabulary's is refused:
    it reads only through a retarget.
  - `PoseFromStageSample` is the rule a stage reader and an OpenExec node
    share, taking values rather than a prim. It is the library home
    `usd-vrm-plugins` had nowhere to put, which is why that repository carried
    the rule twice.
  - `RootMotion::worldOrientation` is carried. The hips rotation is the body's
    orientation as well as the local rotation (MOTION_CONTRACT.md §5.3), and
    both of the copies it arrived from kept only the place.
  - The skeleton comes back as the two arrays `BuildSkeletonDescriptor`
    takes — and its descriptor is what `BuildSourceRestPose` takes after it —
    so reading a stage links no retargeter.
  - `customData.motion.nominalFrameRate` is read back onto the clip. It is
    the producer's rate and the stage's `timeCodesPerSecond` is not: a 60 Hz
    capture is authored at 30 (§4.1), and a reader that took the encoding
    would report a measurement nobody made.
  - The exit codes stayed behind: they classify an input for a CLI.
  - `motionUsd/MotionStage.h` now holds `MotionStageContractVersion` and
    `MotionStageTimeCodesPerSecond`, so the reading half does not include the
    writer's header to learn the contract version.

- **The recorded-source layer, imported from `usd-vrm-plugins` with its
  history (vrm MIG-3)**, ahead of the v0.4.0 release that carries it. This
  was the user's call on 2026-09-19. It depends only on `motionCore` and
  `motionUsd`, both of which have arrived. 55 commits came through
  `git filter-repo`: `motionSource`, `motionBvh` with its corpus, the BVH
  tools, the two producer profiles, and the profile check with its invented
  rig. The tools were split in a move of their own, and the rename followed:
  - `motionSource` is in `openstrata::motion` and `motionBvh` in
    `openstrata::motion::bvh` (§23's subnamespaces: a format stays out of the
    core namespace).
  - `motion_bvh_convert` is `motion_convert`, and
    `USDVRM_MOTION_PROFILE_PATH` is `USDMOTION_PROFILE_PATH`.
  - The profiles install to `share/usd-motion-plugins/profiles/motion/`.
  - DIAG-O1 is decided: codes are named, and `VRM_BVH_*` is `MOTION_BVH_*`
    (design policy §42.8, eleven codes in DIAGNOSTICS.md).
  - `bone` stays `bone` in the profile format, where a joint is the source's
    and a bone is the canonical slot it binds to.
- **`motion_convert` authors through `motionUsd`**, which gained the
  producer's rest (`MotionStageOptions::rest`) and a recorded source's
  provenance (`customData.source`). Its stage is the mapping's: `/Animation`,
  30 time codes per second, and `customData.motion`. `--clip-name` is gone,
  because the mapping names the prim `Body`.

- **`motionRetarget`, imported from `usd-vrm-plugins` with its history**
  (vrm MIG-2), ahead of the v0.2.0 release that carries it. 32 commits came
  through `git filter-repo`: the generic half of `vrmRetarget`, cut by header,
  with the VRM half (`ExpressionResolver`, `LookAtEvaluator`) left out of the
  history. A move-only commit and the rename followed:
  - `TargetSkeleton` is `SkeletonDescriptor`, `TargetJoint` is
    `SkeletonJoint`, and `HumanoidMap` is `RetargetMap`, in
    `openstrata::motion`.
  - The eight `VRM_RETARGET_*` codes are `MOTION_RETARGET_*`, each event name
    unchanged (DIAGNOSTICS.md §2.2).
  - 24 of the unit suite's 56 tests arrived: every one about the body
    retarget. The rest test code that stayed behind.

  Then, each in a change of its own:
  - WS-O2 is decided: `motionRetarget` depends on `motionCore` alone.
    `RetargetOptions::resampleRate` is gone, and a clip is retargeted at its
    own sample times.
  - The required-bone set is the caller's, as `RetargetOptions::requiredBones`,
    empty by default. `RetargetMap::GetRequiredBones`, which stated VRM 1.0's
    set, is gone. Under `Hips` root motion the hips are required regardless.
  - `BuildSkeletonDescriptor` builds a skeleton from joint tokens and rest
    matrices, and `BuildSourceRestPose` reads a clip's rest pose off its
    semantic skeleton (RETARGETING_POLICY.md §10).
  - RT-O1 is decided: the published root-motion vocabulary is the imported
    `Hips` / `RootJoint` / `Ignore`.

- **`motion_record`, imported from `usd-vrm-plugins`' `motion_capture` with
  its history** (vrm MIG-4, its first item), ahead of the v0.3.0 release that
  carries it. 17 commits came through `git filter-repo`, without the clip
  writer, which had already arrived as `motionUsd`. A move-only commit into
  `tools/motionRecord/` and the rename followed:
  - It authors through `motionUsd`: the mapping's `/Animation` stage, with the
    capture rest, `customData.motion.sourceFormat` `capture`, and the session's
    provenance and intake settings as `customData.source`. Its own writer and
    `--clip-name` are gone.
  - `--missing-bones` is `--missing-joints`.
  - A session whose channels or look-at targets the stage cannot hold yet
    (USD-O4) says so on stderr.
  - `motion_record_replay` replays `motionRecording`'s corpus. The leg that
    baked the result onto a VRM avatar stays in `usd-vrm-plugins`; here the
    stage is resolved through a `UsdSkelSkeletonQuery` instead.

- **`execMotion`, imported from `usd-vrm-plugins` with its history** (vrm
  MIG-2, its last item), ahead of the v0.5.0 release that carries it. 13
  commits came through `git filter-repo`, into the directory this workspace
  reserved, and the rename and the workspace join followed. It is the first
  bundle here, and the only member that cannot build without OpenExec:
  - Eight computations over `UsdSkelAnimation` and one attribute expression:
    `motion.identityPose`, `motion.sampleAnimation`, `motion.filterPose` with
    the `motion.priorPose` seam a driver overrides, `motion.extractRootMotion`,
    `motion.interpolatePose` over `motion.poseHistory`, `motion.blendPoses`
    over a relationship, and `motion:root:transform`, which places an
    Xformable through `usdExecImaging`.
  - Each node is a library call, because the four findings it raised were
    fixed before it arrived: `PoseFilter::Step`, `ConditionRootMotion`,
    `SampleClip` and the N-way `BlendPoses`' `std::optional`. What the library
    answers where a node refuses is pinned in `execMotion_pose`.
  - `USDMOTION_BUILD_EXEC_MOTION` (ON) turns it off, and the OpenExec probe is
    `usdmotion_require_openexec()` in `cmake/UsdMotionOpenUsd.cmake`, called by
    this bundle alone — so every library and tool still builds against a
    runtime with no OpenExec.
  - EX-O2 is decided with it: the rate stays a namespaced convention.
  - CI gained one cell, `execmotion-pr-linux`: the standalone
    `ost plugin build` path and the L0–L5 pyramid, whose golden roundtrip over
    eight fixtures no CTest suite runs.

- **USD-O4 is decided**
  ([USD_MAPPING.md §4.3](docs/design/USD_MAPPING.md#43-channels)): a channel is
  one prim under `/Animation/Channels`, carrying `motion:channelName` — the
  namespaced semantic verbatim, and the key, because a sanitized prim path can
  differ from the name — and a time-sampled `float motion:channelValue`. The
  prim is typeless, a writer must make the prim names unique rather than let
  one channel overwrite another, and a reader keys on the attribute.
  `vrm:expressionType` does not come across: a format's classification of its
  own channel belongs in that format's namespace.

- **`execMotion` calls `PoseFromStageSample` and holds no copy of it**
  (USD_MAPPING.md §7.1). The bundle carried the sampling rule because it lived
  in `usd-vrm-plugins`' retarget CLI and a computation cannot call a CLI; the
  reading half's arrival ended that, and `WORKSPACE.md` §2.1 gains the edge.
  The switch **changes what the nodes that read `RootMotion` answer**, which
  is the point of it (USD_MAPPING.md §7.1 lists them):
  - `motion.filterPose` smooths the root orientation. The shared rule sets
    `root.hasOrientation` from the hips rotation (MOTION_CONTRACT.md §5.3),
    where the copy left it false, so `motion:filter:rootOrientation` stopped
    being inert — and it defaults to true, so a clip authoring no policy is
    affected. `execMotion_pose` pins the default and the explicit refusal.
  - `motion.interpolatePose` slerps it between two bracketing samples, where
    before neither sample carried one.
  - `motion.extractRootMotion` returns a root motion carrying that
    orientation. `ConditionRootMotion` does not branch on it, so only the
    value is fuller.
  - `motion.rootTransform` rotates a placement for a clip that turns its
    hips. `motion.blendPoses` is unaffected: `BlendPoses` does not read
    `root`.

  The eight L0–L5 goldens are unchanged, because no fixture turns its hips.
  The bundle links `motionUsd`, which carries OpenUSD's `usdGeom` in
  transitively; `execMotion_boundaries` allows it and says why.

- **`motionCore` gains `FindHumanJointByPath`**, `HumanJointPath`'s inverse.
  The leaf-segment rule had a copy in `motionUsd`, one in `execMotion` and two
  more in `usd-vrm-plugins`; the vocabulary owns the spelling of its own
  paths, so the rule lives beside it and the copies are gone.

- **The `Channels` prim is authored and read** (USD_MAPPING.md §4.3), with the
  reading half. One typeless prim per channel under `/Animation/Channels`, the
  semantic verbatim on `uniform string motion:channelName` and the value on a
  time-sampled `float motion:channelValue`. Two semantics that sanitize to one
  prim name are refused rather than authored over each other, and a channel is
  read back only at an instant the stage keyed it — USD holds the last key
  forward, and reading at every body key would give later samples a value the
  producer never reported. `MotionStageReport::unauthoredChannels` is
  `channels`, the ones it wrote, and `motion_record` counts them instead of
  warning that they were dropped. This does not bump `contractVersion`: a
  consumer that read a stage without a `Channels` prim is unaffected by one
  gaining it (§8).

### Changed

- **The `ost` pin is 0.23.1**, re-pinned across the ecosystem together with
  `usd-vrm-plugins` and `motion-connectors`, and the workflow re-rendered from
  it. The bump is taken for what it adds: `requires.libraries` can name a
  digest-pinned library artifact from another repository, and every rendered
  job runs `ost library pull` before it builds. That is the edge this
  repository's packages are consumed over, so the pin is a precondition of
  publishing them (`usd-vrm-plugins`' ost report 41). 0.23.0 also lets an
  explicitly empty workspace render CI, which this repository needed before
  its first member and the next repository to start empty still does. The
  pinned runtime leaves do not move.

  0.23.1 and not 0.23.0: 0.23.0's new `consumer-link` claim probed a
  materialized runtime before the relocation `ost configure` and
  `ost plugin build` apply to that same prefix, so this repository's hosted
  Linux and Windows lanes went red on the pin bump alone. Measured, reported
  as `usd-vrm-plugins`' ost report 42 and fixed upstream the same day.
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
