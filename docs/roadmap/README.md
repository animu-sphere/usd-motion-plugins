# Roadmap

The roadmap holds only **incomplete** work. When something lands, its detail
leaves this directory: shipped scope goes to the changelog and a release
record, and the implemented state to [architecture/](../architecture/) and
[reference/](../reference/). Rationale lives in [design/](../design/).

Legend: ✅ done · 🚧 in progress · ⬜ not started · ⛔ blocked

| Document | Contents |
| --- | --- |
| [current.md](current.md) | What is left after the imports: the first consumer that has never heard of VRM, and the work beyond the imports. |

## Status at a glance

`v0.5.0` is published. Its release scope is recorded in the
[release record](../releases/v0.5.0.md), and current implementation facts are
in the [capability matrix](../reference/CAPABILITY_MATRIX.md). This directory
contains no completed release inventory.

## Open decisions

Every open question the design documents carry, in the order they block work.
The owning document holds the question; this list only schedules it.

| Id | Question | Owner | Blocks |
| --- | --- | --- | --- |
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
