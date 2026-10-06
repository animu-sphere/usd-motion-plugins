# Roadmap

The roadmap holds only **incomplete** work. When something lands, its detail
leaves this directory: shipped scope goes to the changelog and a release
record, and the implemented state to [architecture/](../architecture/) and
[reference/](../reference/). Rationale lives in [design/](../design/).

Legend: ✅ done · 🚧 in progress · ⬜ not started · ⛔ blocked

| Document | Contents |
| --- | --- |
| [current.md](current.md) | Incomplete work after the imports: runtime and connector boundaries, scene-side motion bindings and later features. |
| [runtime-boundary.md](runtime-boundary.md) | Runtime Boundary Phase 1-5: validation ownership, USD skeleton/rest readers, StageClip absorption, recording and API/ABI stabilization. |
| [connector-boundary.md](connector-boundary.md) | Connector Boundary Phase A-D: canonical intake, external live bridge consumption, generic tracker solve evaluation and dependency enforcement. |

## Status at a glance

`v0.5.3` is published. Its release scope is recorded in the
[release record](../releases/v0.5.3.md), and current implementation facts are
in the [capability matrix](../reference/CAPABILITY_MATRIX.md). This directory
contains no completed release inventory.

## Open decisions

Every open question the design documents carry, in the order they block work.
The owning document holds the question; this list only schedules it.

| Id | Question | Owner | Blocks |
| --- | --- | --- | --- |
| WS-O4 | How `motionUsd` obtains descriptor/rest types and builders | [WORKSPACE §2.5](../architecture/WORKSPACE.md#25-runtime-boundary-target) | Runtime Boundary Phase 2 |
| MC-O7 | Motion-owned intake metadata and generic restart/discontinuity/missing/stale input state | [MOTION §9.1](../design/MOTION_CONTRACT.md#91-canonical-intake-and-acquisition-envelopes) | Connector Boundary Phases A-B |
| MC-O8 | Conditional generic tracker solve ownership and motion-owned input/component | [MOTION §11.1](../design/MOTION_CONTRACT.md#111-a-tracker-observation-gets-no-type-here) | Connector Boundary Phase C |
| MC-O2 | Per-joint translations | [MOTION §13](../design/MOTION_CONTRACT.md#13-open-questions) | a producer |
| MC-O3 | Two-channel root motion (VMC) | [MOTION §13](../design/MOTION_CONTRACT.md#13-open-questions) | a recorded session |
| MC-O6 | Tracking state | [MOTION §13](../design/MOTION_CONTRACT.md#13-open-questions) | a live producer |
| MC-O4 | A non-scalar channel's value type | [MOTION §13](../design/MOTION_CONTRACT.md#13-open-questions) | the first non-scalar channel |
| EX-O3 | Scene-side evaluation attributes before or with `Bindings` | [EXEC §7](../design/EXEC_CONTRACT.md#7-open-questions) | USD-O5 |
| EX-O1 | Where the exec driver lives | [EXEC §7](../design/EXEC_CONTRACT.md#7-open-questions) | a second caller |
| RT-O4 | Bind transforms in the descriptor | [RETARGET §9](../design/RETARGETING_POLICY.md#9-open-questions) | a consumer |
| USD-O3 | Explicit root orientation and velocities | [USD §9](../design/USD_MAPPING.md#9-open-questions) | a consumer |
| USD-O5 | The `Bindings` prim | [USD §9](../design/USD_MAPPING.md#9-open-questions) | `usd-avatar-runtime` |
| USD-O6 | A directly opened `.vmd` | [USD §9](../design/USD_MAPPING.md#9-open-questions) | `usd-mmd-plugins` |
| WS-O3 | A BVH file-format bundle | [WORKSPACE §6](../architecture/WORKSPACE.md#6-open-questions) | a consumer |
| WS-O5 | Neutral public value types for full OpenUSD isolation | [WORKSPACE §2.5](../architecture/WORKSPACE.md#25-runtime-boundary-target) | Runtime Boundary Phase 5, before ABI freeze |
