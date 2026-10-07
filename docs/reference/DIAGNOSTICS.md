# Diagnostics

The catalog of diagnostic codes this repository raises. Status (2026-10-07):
eleven `MOTION_BVH_*`, eight `MOTION_RETARGET_*`, and sixteen
`MOTION_VALIDATION_*` codes, plus the strict USD-reader codes below.
A code is added here in the change that first raises it.

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

### 2.3 Validation

`motionCore/Validation.h` owns `ValidationCode`, `ValidationReport` and the
stable string table exposed by `ValidationCodeString`. All sixteen codes have
severity **error** and are **not recoverable**; severity/recoverability are
fixed by this contract rather than stored per record. `ValidationDiagnostic`
carries code, subject and detail. Each code/subject appears once in first-raised
order. Composite reports prefix field subjects; prose is not a stable API.

`motionCore` raises pose/clip errors; `motionRetarget` reuses the primitive
checks for rest values and raises skeleton/configuration errors. Retarget
warnings remain in the existing §2.2 catalog and `RetargetValidationReport`'s
separate `diagnostics` list. A missing required bone or unavailable root joint
does not become a malformed-input error.

| Code | Meaning |
| --- | --- |
| `MOTION_VALIDATION_NON_FINITE_VALUE` | a present scalar/vector, clip metadata or adjacent timestamp difference is not finite |
| `MOTION_VALIDATION_INVALID_QUATERNION` | a present quaternion is zero or has a non-finite component |
| `MOTION_VALIDATION_NON_UNIT_QUATERNION` | a quaternion violates the explicitly selected unit policy |
| `MOTION_VALIDATION_TIMESTAMP_ORDER` | clip sample times violate the selected non-decreasing or strictly increasing policy |
| `MOTION_VALIDATION_EMPTY_CHANNEL_NAME` | a channel names nothing |
| `MOTION_VALIDATION_CHANNEL_ORDER` | channel names are not sorted |
| `MOTION_VALIDATION_DUPLICATE_CHANNEL` | a channel name is repeated |
| `MOTION_VALIDATION_CONFIDENCE_RANGE` | a confidence value is not finite or outside `[0,1]` |
| `MOTION_VALIDATION_INVALID_ENUM` | a value or validation option is outside its vocabulary |
| `MOTION_VALIDATION_INVALID_PARENT` | a parent is outside its hierarchy and is not its no-parent sentinel |
| `MOTION_VALIDATION_HIERARCHY_CYCLE` | a joint participates in a parent cycle |
| `MOTION_VALIDATION_HIERARCHY_ORDER` | a skeleton's parent does not precede its child |
| `MOTION_VALIDATION_EMPTY_JOINT_TOKEN` | a skeleton joint names nothing |
| `MOTION_VALIDATION_DUPLICATE_JOINT_TOKEN` | a skeleton joint token is repeated |
| `MOTION_VALIDATION_INVALID_JOINT_INDEX` | a map's target index is outside the supplied skeleton |
| `MOTION_VALIDATION_TARGET_REST_SIZE` | a target reference rest has more slots than the skeleton |

### 2.4 Strict USD readers

`SkeletonReadDiagnostic` carries one refusal code, subject and detail. These
codes denote errors and are not recoverable for the selected strict profile;
the destination is unchanged. The stage is never modified. Owner value-validator
failures forward the first §2.3 code/subject/detail without recoding.
Permissive clip-reader warnings remain strings, separate from this surface.

| Code | Subject | Meaning |
| --- | --- | --- |
| `MOTION_USD_OUTPUT` | selected skeleton path | null destination |
| `MOTION_USD_STAGE` | selected skeleton path | null stage |
| `MOTION_USD_SKELETON_PATH` | selected path | expected an absolute prim path without variant selections |
| `MOTION_USD_SKELETON` | selected path | no skeleton at that path |
| `MOTION_USD_UP_AXIS` | skeleton path | strict profile requires Y-up |
| `MOTION_USD_UNITS` | skeleton path | nonpositive/nonfinite units, or canonical clip units are not metres |
| `MOTION_USD_JOINTS` | skeleton path | missing, empty or unrepresentable joint array |
| `MOTION_USD_REST_COUNT` | skeleton path | missing default rest or not one rest per joint |
| `MOTION_USD_JOINT_TOKEN` | joint token | empty, duplicate or malformed relative joint path |
| `MOTION_USD_NONFINITE` | joint token or skeleton path | rest/placement matrix is not finite |
| `MOTION_USD_NONAFFINE` | joint token or skeleton path | perspective exceeds accepted affine roundoff |
| `MOTION_USD_SCALE` | joint token or skeleton path | scale is not positive and float-representable |
| `MOTION_USD_PLACEMENT_SCALE` | skeleton path | placement scale is not rigid |
| `MOTION_USD_SHEAR` | joint token or skeleton path | rest/placement shear exceeds tolerance |
| `MOTION_USD_REFLECTION` | joint token or skeleton path | rest/placement reflects its basis |
| `MOTION_USD_FLOAT_RANGE` | joint token | metre rest translation is not float-representable |
| `MOTION_USD_TOPOLOGY` | skeleton path | resolved topology is invalid |
| `MOTION_USD_PARENT_MAPPING` | joint token | topology disagrees with the immediate-parent rest convention |
| `MOTION_USD_PLACEMENT_RANGE` | skeleton path | metre placement translation is not finite |
| `MOTION_USD_RATE` | skeleton path | canonical clip encoding rate is not positive and finite |
| `MOTION_USD_PLACEMENT` | skeleton path | canonical clip requires identity placement |
| `MOTION_USD_READ` | skeleton path | common clip reader refused; detail retains its reason |
| `MOTION_USD_SKELETON_ROLE` | skeleton path | role is outside the generic/semantic-source vocabulary |
| `MOTION_USD_DESCRIPTOR` | skeleton path | owner builder did not produce a descriptor from validated arrays |
| `MOTION_USD_SOURCE_REST_NO_HUMAN_BONE` | skeleton path | explicitly semantic source names no vocabulary bone |
| `MOTION_USD_SOURCE_REST_DUPLICATE_BONE` | first offending joint token | explicitly semantic source names a bone more than once |

## 3. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| ~~DIAG-O1~~ | **Decided 2026-09-19: named codes, and an imported code is renamed on arrival** (`VRM_BVH_*` → `MOTION_BVH_*`, the event name unchanged). Recorded as design policy §42.8. Was: Code style. The design policy's §29 proposes numbered codes (`MOTION-E0001`, `MOTION-W…`, `MOTION-I…`); the imported code and both sibling repositories use named codes (`VRM_RETARGET_UNBOUND_DRIVEN_BONE`, `MMD_MOTION_UNMATCHED_BONE`) whose name is the event. `usd-mmd-plugins` already documents passing `MOTION-*` codes through unchanged. Numbered or named, and whether imported codes are renamed on arrival | the first imported diagnostic (v0.1.0) |
