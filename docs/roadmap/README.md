# Roadmap

The roadmap holds only **incomplete** work. When something lands, its detail
leaves this directory: shipped scope goes to the changelog and a release
record, and the implemented state to [architecture/](../architecture/) and
[reference/](../reference/). Rationale lives in [design/](../design/).

Legend: ✅ done · 🚧 in progress · ⬜ not started · ⛔ blocked

| Document | Contents |
| --- | --- |
| [current.md](current.md) | The current milestone, the first import, and what is left of the scaffold. |

## Two sequences

| Sequence | What it tracks | Source of truth |
| --- | --- | --- |
| **Releases v0.1.0–v0.5.0** | what this repository delivers | [DESIGN_POLICY.md §35](../design/DESIGN_POLICY.md#35-release-strategy), as mapped below |
| **Migration Phase A–F** | how generic motion leaves `usd-vrm-plugins` and `usd-mmd-plugins` joins | [DESIGN_POLICY.md §37](../design/DESIGN_POLICY.md#37-migration-from-usd-vrm-plugins) |

Migration phases are always written with the qualifier
([DESIGN_POLICY.md §42.3](../design/DESIGN_POLICY.md#423-migration-phases-are-always-qualified)).
`usd-vrm-plugins` tracks its side as MIG-0 to MIG-5 in its migration track,
and `usd-mmd-plugins` as its Phase 9.

## Status at a glance

**This table is the single source of truth for which release carries what.**

| Release | Scope | Imports | Migration Phase | Status |
| --- | --- | --- | --- | --- |
| v0.1.0 — core contract | `motionCore`, `motionSampling`, `motionRecording`; `motionUsd` authoring a motion stage; deterministic tests | `motionCore`, `motionRuntime`, the authoring half of `StageIo` (vrm MIG-1, part of MIG-2) | A, B | ⬜ |
| v0.2.0 — retargeting | `motionRetarget`; `motionUsd` reading; mapping validation; the VRM and MMD integration hooks | `vrmRetarget`'s generic half, the reading half of `StageIo` (vrm MIG-2) | C | ⬜ |
| v0.3.0 — recording and stream utilities | the published `MotionStream` shape (MC-O5); `motion_record`; the processor interface | `motion_capture` (vrm MIG-4, its first item) | E | ⬜ |
| v0.4.0 — generic format integration | `motionSource`, `motionBvh`, `motion_convert`, `motion_bvh_inspect`, producer profiles | vrm MIG-3 | C | ⬜ |
| v0.5.0 — runtime integration | `execMotion` | vrm MIG-2, its last item | — | ⬜ |
| later | generic NPZ payload contract, IK-assisted retarget, contacts, blending beyond the imported one, generator interfaces, Python | — | F follows the imports | ⬜ |

**Where this departs from the design policy's §35, and why.** The code
arrives by moving whole identities, and moving half of one would leave
`usd-vrm-plugins` with two copies of a library across releases, which its
moving rules forbid. Two consequences:

- `motionRuntime` holds sampling and capture together, so **recording arrives
  in v0.1.0**, not v0.3.0; v0.3.0 keeps what is new — the published stream
  shape and the recording tool.
- `SkeletonDescriptor` and the basic `RetargetMap` are part of the retargeter,
  so they **arrive in v0.2.0** with it, not in v0.1.0. v0.2.0 then delivers
  §35's v0.1.0 retarget items and its own v0.2.0 list at once, because the
  imported retargeter is already rest-pose-aware.

**Migration Phase D** — `usd-mmd-plugins` consuming the core — is that
repository's Phase 9. Its adapter needs v0.1.0 and uses v0.2.0's map
validation; its evaluator needs nothing from here.

## Open decisions

Every open question the design documents carry, in the order they block work.
The owning document holds the question; this list only schedules it. MC-O1,
USD-O1 and USD-O2 were decided on 2026-09-19, and MC-O4 narrowed to its
non-scalar case, before the first import. RT-O2 and RT-O3 were carried the same
day from `usd-vrm-plugins`' v0.9.0 decisions.

| Id | Question | Owner | Blocks |
| --- | --- | --- | --- |
| DIAG-O1 | Diagnostic code style | [DIAGNOSTICS §3](../reference/DIAGNOSTICS.md#3-open-questions) | the first imported diagnostic |
| WS-O2 | `motionRetarget`'s edge to `motionSampling` | [WORKSPACE §6](../architecture/WORKSPACE.md#6-open-questions) | v0.2.0 |
| RT-O1 | Root-motion vocabulary | [RETARGET §9](../design/RETARGETING_POLICY.md#9-open-questions) | v0.2.0 |
| USD-O4 | Channel attribute names | [USD §9](../design/USD_MAPPING.md#9-open-questions) | v0.2.0 |
| MC-O5 | `MotionStream`'s public shape | [MOTION §13](../design/MOTION_CONTRACT.md#13-open-questions) | v0.3.0 |
| MC-O2 | Per-joint translations | [MOTION §13](../design/MOTION_CONTRACT.md#13-open-questions) | a producer |
| MC-O3 | Two-channel root motion (VMC) | [MOTION §13](../design/MOTION_CONTRACT.md#13-open-questions) | a recorded session |
| MC-O6 | Tracking state | [MOTION §13](../design/MOTION_CONTRACT.md#13-open-questions) | a live producer |
| MC-O4 | A non-scalar channel's value type | [MOTION §13](../design/MOTION_CONTRACT.md#13-open-questions) | the first non-scalar channel |
| EX-O2 | The rate attribute: schema or namespaced convention | [EXEC §7](../design/EXEC_CONTRACT.md#7-open-questions) | the import of `execMotion` |
| EX-O3 | Scene-side evaluation attributes before or with `Bindings` | [EXEC §7](../design/EXEC_CONTRACT.md#7-open-questions) | USD-O5 |
| EX-O1 | Where the exec driver lives | [EXEC §7](../design/EXEC_CONTRACT.md#7-open-questions) | a second caller |
| RT-O4 | Bind transforms in the descriptor | [RETARGET §9](../design/RETARGETING_POLICY.md#9-open-questions) | a consumer |
| USD-O3 | Explicit root orientation and velocities | [USD §9](../design/USD_MAPPING.md#9-open-questions) | a consumer |
| USD-O5 | The `Bindings` prim | [USD §9](../design/USD_MAPPING.md#9-open-questions) | `usd-avatar-runtime` |
| USD-O6 | A directly opened `.vmd` | [USD §9](../design/USD_MAPPING.md#9-open-questions) | `usd-mmd-plugins` |
| WS-O3 | A BVH file-format bundle | [WORKSPACE §6](../architecture/WORKSPACE.md#6-open-questions) | a consumer |
