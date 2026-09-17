# Current — the scaffold, then v0.1.0

Status: 🚧 documentation baseline done (2026-09-17); everything below ⬜.

Migration Phase A is *define the public contracts*
([DESIGN_POLICY.md §37](../design/DESIGN_POLICY.md#37-migration-from-usd-vrm-plugins)).
Its documents exist — the design policy, the motion contract, the retargeting
policy, the USD mapping and the workspace contract — as **proposed**
contracts written from `usd-vrm-plugins`' measured implementation. What
remains is a tree that can receive code, and the first import.

## What remains

### The scaffold ⬜ — can start now

- ⬜ Resolve **WS-O1**, target and package names.
- ⬜ Root `CMakeLists.txt`, `CMakePresets.json`, `VERSION` (`0.1.0` in
  development), `openstrata.toml`, `openstrata.ci.yaml` with the siblings'
  OpenUSD 26.08 runtimes and `ost` pin, and a generated CI workflow.
- ⬜ A docs check in CI: every relative link and anchor resolves, and every
  version mirror agrees with `VERSION`, as the siblings' `check_docs.py` do.
- ⬜ An installed-consumer lane that configures the (empty) packages from a
  clean prefix outside the repository, so the first import lands into a lane
  that already runs.
- ⬜ Community files matching the siblings: `CONTRIBUTING.md`,
  `CODE_OF_CONDUCT.md`, `SECURITY.md`, issue and pull request templates.

### v0.1.0 — the core contract ⛔ on `usd-vrm-plugins` v0.9.0

`usd-vrm-plugins` finishes its OpenExec foundation before anything moves,
because that work's findings are the API defects fixed on arrival.

- ⬜ Resolve **MC-O1** (joint vocabulary), **USD-O1** (prim names), **USD-O2**
  (time codes) and **MC-O4** (channel value type) — each before the code that
  would freeze it.
- ⬜ Import `motionCore` as `motion-core`, with history, renamed
  ([WORKSPACE.md §3](../architecture/WORKSPACE.md#3-moving-code-in)).
- ⬜ Import `motionRuntime` as `motion-sampling` and `motion-recording`.
- ⬜ `motion-usd`: author the motion stage of
  [USD_MAPPING.md §2–§5](../design/USD_MAPPING.md#2-the-standalone-motion-stage),
  identity `scales` included, and open it through OpenUSD in a test.
- ⬜ Fix the sampling findings in their own change
  ([MOTION_CONTRACT.md §8](../design/MOTION_CONTRACT.md#8-motionclip-and-sampling)).
- ⬜ Reproduce the parity evidence named by `usd-vrm-plugins`' MIG-0 against
  this repository's packages, so that repository can delete its copies.
- ⬜ `usd-mmd-plugins`' `mmdMotionAdapter` configures against the installed
  `motion-core` — the first consumer that has never heard of VRM.

## Completion criteria

v0.1.0 is done when `MotionPose`, `MotionClip`, `HumanJoint`, sampling with
status, recording, `UsdSkelAnimation` authoring and deterministic tests work
from installed packages, and `usd-vrm-plugins` builds against them with its
`motionCore` and `motionRuntime` deleted. `SkeletonDescriptor` and the retarget
map, which design policy §35 also lists here, arrive with the retargeter in
v0.2.0 ([roadmap README](README.md#status-at-a-glance)).
