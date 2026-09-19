# Current — the scaffold, then v0.1.0

Status: 🚧 documentation baseline done (2026-09-17); scaffold done
(2026-09-19) except its rendered CI; v0.1.0 ⬜, and no longer blocked.

Migration Phase A is *define the public contracts*
([DESIGN_POLICY.md §37](../design/DESIGN_POLICY.md#37-migration-from-usd-vrm-plugins)).
Its documents exist — the design policy, the motion contract, the retargeting
policy, the USD mapping and the workspace contract — as **proposed**
contracts written from `usd-vrm-plugins`' measured implementation. The tree
can now receive code. What remains is the first import.

## What remains

### The scaffold ✅ (2026-09-19), except the rendered CI

The names are decided (WS-O1:
[WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-bundles-tools-and-data)).
The root project pins OpenUSD 26.08, the docs check gates every pull request,
the installed-consumer lane runs over an empty package list, and the community
files are in place. What landed is in the [changelog](../../CHANGELOG.md), and
how to build it is in the [building guide](../guides/building.md).

- ⛔ **The rendered `ost` CI workflow.** `openstrata.ci.yaml` holds the three
  workspace cells. Under `ost` 0.22.10, though, every rendered job runs
  `ost plugin test --workspace --graph-only`, and that refuses a workspace with
  no member, so the workflow is rendered in the first import's change, together
  with the graph cell. Until then the cells are run by hand with `ost build` and
  `ost test`. An `ost` that let an empty workspace's graph step pass or skip
  would unblock it, and that is an upstream request.

### v0.1.0 — the core contract ⬜

`usd-vrm-plugins` v0.9.0, the OpenExec foundation, was published on 2026-09-17.
Its findings are the API defects fixed on arrival. That repository's MIG-0
has drawn the line through `vrmRetarget` and lists, in a checked ledger, every
VRM-vocabulary name the moving headers still spell (its WORKSPACE.md §9.3 and
§9.5).

- ⬜ Resolve **MC-O1** (joint vocabulary), **USD-O1** (prim names), **USD-O2**
  (time codes) and **MC-O4** (channel value type) — each before the code that
  would freeze it.
- ⬜ Import `motionCore` as `motionCore`, with history, renamed
  ([WORKSPACE.md §3](../architecture/WORKSPACE.md#3-moving-code-in)).
- ⬜ Import `motionRuntime` as `motionSampling` and `motionRecording`.
- ⬜ `motionUsd`: author the motion stage of
  [USD_MAPPING.md §2–§5](../design/USD_MAPPING.md#2-the-standalone-motion-stage),
  identity `scales` included, and open it through OpenUSD in a test.
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
