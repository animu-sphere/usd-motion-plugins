# Current — the scaffold, then v0.1.0

Status: 🚧 documentation baseline done (2026-09-17); scaffold done
(2026-09-19), its CI rendered with the first import; v0.1.0 🚧 — `motionCore`,
`motionSampling` and `motionRecording` imported (2026-09-19).

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
- ⬜ Fix the pose's shape to the contract's after the move: `source` becomes
  the non-optional `metadata` (§5.1), and `SourceMetadata` gains
  `sourceTimestamp` and `sequenceNumber` (§7).
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
- ⬜ `motionUsd`: author the motion stage of
  [USD_MAPPING.md §2–§5](../design/USD_MAPPING.md#2-the-standalone-motion-stage),
  identity `scales` and the `motion:timeCodesPerSecond` rate included
  ([EXEC_CONTRACT.md §5.1](../design/EXEC_CONTRACT.md#51-the-rate-motiontimecodespersecond)),
  and open it through OpenUSD in a test.
- ⬜ Fix the sampling findings in their own change
  ([MOTION_CONTRACT.md §8](../design/MOTION_CONTRACT.md#8-motionclip-and-sampling)).
- ⬜ Reproduce the parity evidence named by `usd-vrm-plugins`' MIG-0 against
  this repository's packages, so that repository can delete its copies.
- ⬜ `usd-mmd-plugins`' `mmdMotionAdapter` configures against the installed
  `motionCore` — the first consumer that has never heard of VRM.

## Completion criteria

v0.1.0 is done when `MotionPose`, `MotionClip`, `HumanJoint`, sampling with
status, recording, `UsdSkelAnimation` authoring and deterministic tests work
from installed packages, and `usd-vrm-plugins` builds against them with its
`motionCore` and `motionRuntime` deleted. `SkeletonDescriptor` and the retarget
map, which design policy §35 also lists here, arrive with the retargeter in
v0.2.0 ([roadmap README](README.md#status-at-a-glance)).
