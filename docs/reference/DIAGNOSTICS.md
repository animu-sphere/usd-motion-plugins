# Diagnostics

The catalog of diagnostic codes this repository raises. Status (2026-09-19):
the eleven `MOTION_BVH_*` codes, which arrived with `motionBvh`, and the eight
`MOTION_RETARGET_*` codes, which arrived with `motionRetarget`. A code is added
here in the change that first raises it.

## 1. The record

As in the sibling repositories and the imported code, a diagnostic is a
**value**: a code, a severity from one table (so a call site cannot choose
it), a subject naming what it is about (a joint, an index, a path, a byte
offset), a detail sentence for a person, and whether it is recoverable. Tests
assert the code and the subject, never the prose. Lists hold each
code-and-subject once, in the order raised
([RETARGETING_POLICY.md §7](../design/RETARGETING_POLICY.md#7-diagnostics)).

Diagnostics must distinguish malformed data, unsupported valid data, missing
optional data, mapping failure, retarget policy conflict and USD authoring
failure (design policy §29). A diagnostic raised by a consumer's code keeps
its own code when it passes through; this repository's codes are never
re-coded by a consumer, and a consumer's never by this repository.

## 2. Catalog

### 2.1 BVH

The first five are syntax, and the parser raises them. The other six are about
a file meeting a profile, and only a caller holding both raises them. The
extractor is granted `MOTION_BVH_INVALID_ROTATION_ORDER` alone
(`libs/motionBvh/tests/check_boundaries.py`). Each code's definition is in
`libs/motionBvh/include/motionBvh/Diagnostics.h`.

| Code | Severity | Recoverable | Raised by | Meaning |
| --- | --- | --- | --- | --- |
| `MOTION_BVH_PARSE_FAILED` | error | no | `motionBvh` parser; `motion_convert` for a rig that is not a rig | the file is not a decodable BVH document, or a limit refused it |
| `MOTION_BVH_UNSUPPORTED_CHANNEL` | error | no | `motionBvh` parser | a `CHANNELS` list names a channel the format model cannot represent |
| `MOTION_BVH_FRAME_WIDTH_MISMATCH` | error | no | `motionBvh` parser | a motion row does not carry one value per declared channel |
| `MOTION_BVH_INVALID_FRAME_TIME` | error | no | `motionBvh` parser | `Frame Time:` is absent, not a number, negative, or zero where an interval is required |
| `MOTION_BVH_NON_FINITE_VALUE` | error | no | `motionBvh` parser | a parsed number is NaN or infinite |
| `MOTION_BVH_PROFILE_REQUIRED` | error | no | `motion_convert` | a conversion was asked for with no profile; there is no default |
| `MOTION_BVH_PROFILE_MISMATCH` | error | no | `motion_convert` | the named profile does not describe this file |
| `MOTION_BVH_UNMAPPED_JOINT` | warning | yes | `motion_convert` | the file carries a joint the profile does not map; the profile's policy decides whether it is fatal |
| `MOTION_BVH_REQUIRED_JOINT_MISSING` | error | no | `motion_convert` | the profile requires a joint the file does not carry |
| `MOTION_BVH_INVALID_ROTATION_ORDER` | error | no | `motionBvh` extractor; `motion_convert` | a joint's rotation channels form no Euler order |
| `MOTION_BVH_INVALID_ROOT_POLICY` | error | no | `motion_convert` | the profile's root policy cannot be applied to this file |

### 2.2 Retarget

The set splits at the layer boundary
([RETARGETING_POLICY.md §7](../design/RETARGETING_POLICY.md#7-diagnostics)).
`motionRetarget` raises the first five from plain values; the other three say
what a stage or a file system added, and only a caller holding one raises
them. `libs/motionRetarget/tests/check_boundaries.py` refuses those three
anywhere in the library but the table that defines them,
`libs/motionRetarget/include/motionRetarget/Diagnostics.h`. The event names
kept `_BONE` on arrival: here a bone is the canonical slot and a joint is the
rig's. Every code but the last is recoverable, because retargeting onto a
partial rig is legal and useful.

| Code | Severity | Recoverable | Raised by | Meaning |
| --- | --- | --- | --- | --- |
| `MOTION_RETARGET_MISSING_REQUIRED_BONE` | warning | yes | `motionRetarget` | a bone the caller requires (and the hips under `Hips` root motion) has no joint on this rig |
| `MOTION_RETARGET_UNBOUND_DRIVEN_BONE` | warning | yes | `motionRetarget` | the clip drives a bone the rig binds no joint for |
| `MOTION_RETARGET_DUPLICATE_TARGET` | warning | yes | `motionRetarget` | two bones are bound to one joint; the later in the vocabulary wins |
| `MOTION_RETARGET_INVALID_HIERARCHY` | warning | yes | `motionRetarget` | the rig's joints are not in parent-before-child order |
| `MOTION_RETARGET_INVALID_ROOT_JOINT` | warning | yes | `motionRetarget` | root motion names a joint the rig does not have, so no root translation was authored |
| `MOTION_RETARGET_NON_UNIT_SCALE` | warning | yes | a caller reading the clip's `scales` | the clip animates scale, which the retarget does not carry |
| `MOTION_RETARGET_TIME_RANGE_DERIVED` | info | yes | a caller holding a stage | the clip states no time samples, so the stage chose the time |
| `MOTION_RETARGET_OUTPUT_COLLIDES_WITH_INPUT` | error | no | a caller writing a file | the output names a layer the retarget read |

## 3. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| ~~DIAG-O1~~ | **Decided 2026-09-19: named codes, and an imported code is renamed on arrival** (`VRM_BVH_*` → `MOTION_BVH_*`, the event name unchanged). Recorded as design policy §42.8. Was: Code style. The design policy's §29 proposes numbered codes (`MOTION-E0001`, `MOTION-W…`, `MOTION-I…`); the imported code and both sibling repositories use named codes (`VRM_RETARGET_UNBOUND_DRIVEN_BONE`, `MMD_MOTION_UNMATCHED_BONE`) whose name is the event. `usd-mmd-plugins` already documents passing `MOTION-*` codes through unchanged. Numbered or named, and whether imported codes are renamed on arrival | the first imported diagnostic (v0.1.0) |
