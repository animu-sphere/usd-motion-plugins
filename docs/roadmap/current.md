# Current — the scaffold, then v0.1.0

Status: 🚧 documentation baseline done (2026-09-17); scaffold done
(2026-09-19), its CI rendered with the first import; v0.1.0 🚧 — `motionCore`,
`motionSampling` and `motionRecording` imported (2026-09-19). Every later
release's identities have arrived ahead of it as well, the last on 2026-09-20:
what remains to publish is the releases themselves.

Migration Phase A is *define the public contracts*
([DESIGN_POLICY.md §37](../design/DESIGN_POLICY.md#37-migration-from-usd-vrm-plugins)).
Its documents exist — the design policy, the motion contract, the retargeting
policy, the USD mapping and the workspace contract — as **proposed**
contracts written from `usd-vrm-plugins`' measured implementation. The tree
has received its first code: `motionCore`, then `motionRuntime` as
`motionSampling` and `motionRecording`, each with its history.

## What remains

### The scaffold ✅ (2026-09-19)

The names are decided (WS-O1:
[WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-bundles-tools-and-data)).
The root project pins OpenUSD 26.08, the docs check gates every pull request,
the installed-consumer lane runs over an empty package list, and the community
files are in place. What landed is in the [changelog](../../CHANGELOG.md), and
how to build it is in the [building guide](../guides/building.md).

- ✅ **The rendered `ost` CI workflow** (2026-09-19, with `motionCore`).
  Under `ost` 0.22.10 every rendered job runs
  `ost plugin test --workspace --graph-only`, which refuses a workspace with no
  member, so the scaffold could not render it; the first member made it
  renderable, and the graph cell arrived with it. An `ost` that let an empty
  workspace's graph step pass or skip is still an upstream request, for the
  next repository that starts empty.

### v0.1.0 — the core contract ⬜

`usd-vrm-plugins` v0.9.0, the OpenExec foundation, was published on 2026-09-17.
Its findings are the API defects fixed on arrival. That repository's MIG-0
has drawn the line through `vrmRetarget` and lists, in a checked ledger, every
VRM-vocabulary name the moving headers still spell (its WORKSPACE.md §9.3 and
§9.5).

- ✅ Resolve **MC-O1** (55 joints), **USD-O1** (`Skeleton` / `Body` /
  `Channels`), **USD-O2** (always 30) and **MC-O4** (scalar channels; the
  non-scalar case stays open) — 2026-09-19, before the code that would freeze
  them.
- ✅ Import `motionCore` as `motionCore`, with history, renamed
  ([WORKSPACE.md §3](../architecture/WORKSPACE.md#3-moving-code-in)) —
  2026-09-19. 23 commits of history arrived through `git filter-repo`; the
  rename is one commit after the move, and the boundary check gained the
  product-name scan. `usd-vrm-plugins` switches to the installed package and
  deletes its copy in its MIG-1, against a release of this one.
- ✅ Fix the pose's shape to the contract's after the move: `source` becomes
  the non-optional `metadata` (§5.1), and `SourceMetadata` gains
  `sourceTimestamp` and `sequenceNumber` (§7) — 2026-09-19, after the
  runtime arrived, so intake, sampling, the recorder and the trace carry the
  two new fields in the same change
  ([MOTION_CONTRACT.md §10](../design/MOTION_CONTRACT.md#10-recording-and-the-trace-format)):
  the trace format is version 4, and the corpus was regenerated for its header
  line alone.
- ✅ Receive the evidence `usd-vrm-plugins`' MIG-0 hands over — 2026-09-19.
  The OpenExec driver contract and the producer conventions are
  [EXEC_CONTRACT.md](../design/EXEC_CONTRACT.md) (proposed); the v0.9.0 scale
  and partial-skeleton decisions and the retarget's exec findings are
  [RETARGETING_POLICY.md](../design/RETARGETING_POLICY.md) §4.1, §6.1 and §10;
  recorded-source provenance and the tracker boundary are
  [MOTION_CONTRACT.md](../design/MOTION_CONTRACT.md) §7.1 and §11.1.
- ✅ Import `motionRuntime` as `motionSampling` and `motionRecording`
  ([WORKSPACE.md §3](../architecture/WORKSPACE.md#3-moving-code-in)) —
  2026-09-19. 28 commits arrived through `git filter-repo`; one move-only
  commit put each file in the library whose role names it, and the rename
  followed. The recorder took its contract name, `MotionRecorder`;
  `LiveCaptureSource` kept its own, because the published `MotionStream`
  shape is MC-O5. The trace format and every committed trace are unchanged.
  `usd-vrm-plugins` deletes its copy in its MIG-2, against a release of this
  one, after re-running the parity rows its MIG-0 named.
- ✅ `motionUsd`: author the motion stage of
  [USD_MAPPING.md §2–§5](../design/USD_MAPPING.md#2-the-standalone-motion-stage),
  identity `scales` and the `motion:timeCodesPerSecond` rate included
  ([EXEC_CONTRACT.md §5.1](../design/EXEC_CONTRACT.md#51-the-rate-motiontimecodespersecond)),
  and open it through OpenUSD in a test — 2026-09-19. The writer arrived with
  its history from `usd-vrm-plugins`' `motion_capture`, which is the writer
  there that authors an avatar-independent clip, and was adapted to the
  mapping after the move. The `Channels` prim's names were decided on
  2026-09-20 (USD-O4) and it is authored with the reading half, below.
- ✅ Fix the sampling findings in their own change
  ([MOTION_CONTRACT.md §8](../design/MOTION_CONTRACT.md#8-motionclip-and-sampling))
  — 2026-09-19. `SampleClip`, `PoseFilter::Step`, an N-way blend that answers
  nullopt when there is nothing to blend, and `ConditionRootMotion`. The
  streaming classes call these functions, so each rule has one
  implementation. `execMotion` arrived on 2026-09-20 and calls them, so the
  wrapper code the findings were about is gone rather than moved; the copy in
  `usd-vrm-plugins` drops it when that repository switches to these
  packages.
- ⬜ Reproduce the parity evidence named by `usd-vrm-plugins`' MIG-0 against
  this repository's packages, so that repository can delete its copies.
- ⬜ `usd-mmd-plugins`' `mmdMotionAdapter` configures against the installed
  `motionCore` — the first consumer that has never heard of VRM.

### Arrived ahead of its release: v0.4.0's recorded sources ✅ (2026-09-19)

`motionSource`, `motionBvh`, `motion_convert`, `motion_bvh_inspect` and the
producer profiles were imported with their history (`usd-vrm-plugins`' MIG-3)
before v0.2.0 and v0.3.0, by the user's call. They depend only on
`motionCore` and `motionUsd`, and importing early shortens the time
`usd-vrm-plugins` holds a second copy. They still ship as the v0.4.0 scope
([README](README.md#status-at-a-glance)). DIAG-O1 was decided with them
(design policy §42.8). `motion_convert` authors through `motionUsd`, which is
why `motionUsd` takes a producer's rest.

### Arrived ahead of its release: v0.2.0's reading half ✅ (2026-09-20)

`motionUsd`'s reading half was imported with its history from
`usd-vrm-plugins`' `motion_retarget` (that repository's MIG-2, its last open
item). 13 commits came through `git filter-repo` over `StageIo.{h,cpp}`; a
move-only commit and the adaptation followed. That file is the stage half of a
VRM retarget CLI, and a file cannot be filtered in two, so both halves arrived
and the VRM half — the avatar reading, the expressions, the look-at, the bake —
was removed in the adapting commit rather than carried
([WORKSPACE.md §3](../architecture/WORKSPACE.md#3-moving-code-in), rule 5).

- ✅ `ReadMotionStage` and `OpenMotionStage`
  ([USD_MAPPING.md §7](../design/USD_MAPPING.md#7-reading-usd-back)): a
  `MotionClip`, the skeleton's tokens and rest transforms, §5's metadata and
  warnings. A skeleton whose tokens are not the vocabulary's is refused,
  because it reads only through a retarget. The exit codes stayed behind:
  they classify an input for a CLI.
- ✅ The three findings §7 named, fixed with the move. One library home
  (`PoseFromStageSample`, taking values so an exec node can call it);
  `RootMotion::worldOrientation` carried, which both of the copies it arrived
  from dropped; and the skeleton answered as the two arrays
  `BuildSkeletonDescriptor` takes, whose descriptor `BuildSourceRestPose`
  takes after it, so reading a stage links no retargeter.
- ✅ The `Channels` prim is authored as well as read, which is the other half
  of USD-O4 and what makes the round trip checkable. A channel is read back
  only where the stage keyed it, because USD holds the last key forward.
- ⬜ `execMotion` still carries `PoseFromClipSample`, the copy this ends.
  Switching it over adds a `motionUsd` edge
  ([WORKSPACE.md §2.1](../architecture/WORKSPACE.md#21-inside-the-repository))
  and is a change of its own.
- ⬜ `usd-vrm-plugins` deletes `StageIo`'s reading half in its consuming
  change, which waits on `ost` (its report 41). The bake stays there.

### Arrived ahead of its release: v0.2.0's retarget ✅ (2026-09-19)

`motionRetarget` was imported with its history (`usd-vrm-plugins`' MIG-2),
the generic half of `vrmRetarget` cut by header along the line that
repository's WORKSPACE.md §9.5 draws. 32 commits arrived through
`git filter-repo`, with `ExpressionResolver` and `LookAtEvaluator` left out of
the history; a move-only commit and the rename followed. It depends only on
`motionCore`, so nothing it needs is missing, and importing it now shortens
the time `usd-vrm-plugins` holds a second copy. It still ships as v0.2.0's
scope, with `motionUsd`'s reading half.

- ✅ WS-O2 and RT-O1 decided by the user: `motionCore` alone (the resample
  option went, and a caller resamples first), and the imported root-motion
  vocabulary.
- ✅ Finding 1 of that repository's §9.5: the required-bone set is the
  caller's (`RetargetOptions::requiredBones`), and the library names no
  format's set.
- ✅ RETARGETING_POLICY.md §10: `BuildSkeletonDescriptor` and
  `BuildSourceRestPose`, the two rules `usd-vrm-plugins` wrote twice.
- ⬜ `usd-vrm-plugins` re-runs its OpenExec parity rows against this package
  before it deletes its copy (its MIG-0 table). That waits on its consuming
  change, which waits on `ost` (its report 41).

### Arrived ahead of its release: v0.3.0's `motion_record` ✅ (2026-09-19)

`usd-vrm-plugins`' `motion_capture` was imported with its history as
`motion_record` (that repository's MIG-4, its first item). 17 commits arrived
through `git filter-repo`, without the clip writer, which had arrived as
`motionUsd` already; a move-only commit and the rename followed. It depends
only on `motionRecording`, `motionSampling` and `motionUsd`, all of which have
arrived, and importing it now shortens the time `usd-vrm-plugins` holds a
second copy. It still ships as v0.3.0's scope, with the published
`MotionStream` shape (MC-O5) and the processor interface.

- ✅ It authors through `motionUsd`, as `motion_convert` does: the mapping's
  stage with the capture rest, and the session's provenance as
  `customData.source`. Its own writer and `--clip-name` are gone.
- ✅ `motion_record_replay` travels (that repository's MIG-0 table). Its last
  leg baked the recorded clip onto a VRM avatar with `motion_retarget`, a
  consumer of this repository, so that leg stays there; here the stage is
  resolved through a `UsdSkelSkeletonQuery`, the claim the leg rested on.
- ⬜ `usd-vrm-plugins` deletes `tools/motionCapture` in its consuming change,
  which waits on `ost` (its report 41).

### Arrived ahead of its release: v0.5.0's `execMotion` ✅ (2026-09-20)

`usd-vrm-plugins`' `execMotion` was imported with its history (that
repository's MIG-2, its last item). 13 commits arrived through
`git filter-repo` into `plugins/execMotion/`, the directory
[WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-bundles-tools-and-data)
reserved, and the rename and the workspace join followed. It is the first
bundle here. It depends only on `motionCore`, `motionSampling` and
`motionRecording`, all of which have arrived, and importing it now shortens
the time `usd-vrm-plugins` holds a second copy. It still ships as v0.5.0's
scope.

- ✅ Each node is one library call, because the four sampling findings this
  bundle raised were fixed before it arrived: `PoseFilter::Step`,
  `ConditionRootMotion`, `SampleClip` and an N-way `BlendPoses` that answers
  `std::optional`. What the library answers where a node refuses is pinned in
  `execMotion_pose`, and it changed with them — nothing weighted is nullopt
  now, and a NaN weight counts as no weight.
- ✅ The one thing the graph still cannot carry is the filter's state.
  `StepResult::state` is richer than the pose it returns, an exec
  computation's value is a pose, and no node publishes the state as a value of
  its own — so a driver hands the result back as the next prior pose and a
  joint returning after a dropout is passed through rather than slerped. The
  difference is measured, not assumed.
- ✅ It is **optional**, and it is the only member that needs OpenExec.
  `usdmotion_require_openexec()` moved into `cmake/UsdMotionOpenUsd.cmake` and
  this bundle alone calls it, so a runtime without the exec libraries still
  builds every library and tool; `USDMOTION_BUILD_EXEC_MOTION=OFF` leaves the
  bundle out.
- ✅ EX-O2 is decided with it
  ([EXEC_CONTRACT.md §5.1](../design/EXEC_CONTRACT.md#51-the-rate-motiontimecodespersecond)):
  `motion:timeCodesPerSecond` stays a namespaced convention and no schema
  registers it.
- ✅ CI gained one cell, `execmotion-pr-linux`, for what a workspace cell
  cannot reach: the standalone `ost plugin build` and the L0–L5 pyramid, whose
  golden roundtrip over eight fixtures no CTest suite runs.
- ⬜ `usd-vrm-plugins` deletes `plugins/execMotion` in its consuming change,
  which waits on `ost` (its report 41). Its `execVrm` stays there and reaches
  these nodes by name, as it does today.

## Completion criteria

v0.1.0 is done when `MotionPose`, `MotionClip`, `HumanJoint`, sampling with
status, recording, `UsdSkelAnimation` authoring and deterministic tests work
from installed packages, and `usd-vrm-plugins` builds against them with its
`motionCore` and `motionRuntime` deleted. `SkeletonDescriptor` and the retarget
map, which design policy §35 also lists here, arrive with the retargeter in
v0.2.0 ([roadmap README](README.md#status-at-a-glance)).
