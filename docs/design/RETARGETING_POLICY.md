# Retargeting policy

> Status: **binding**, implemented by `motionRetarget` since 2026-09-19. The
> generic half of `usd-vrm-plugins`' `vrmRetarget` — the pose
> retargeter, rest correction and root-motion policy — arrived with its
> history and its tests
> ([DESIGN_POLICY.md §42.1](DESIGN_POLICY.md#421-the-core-is-imported-from-usd-vrm-plugins-not-rewritten)).
> Its VRM half — the humanoid map read from `VrmHumanoidAPI`, VRM 1.0's
> required-bone set, expression and look-at resolution — stayed in
> `usd-vrm-plugins`. The §1 signature is still the target shape: the imported
> API is a `PoseRetargeter` built once from the descriptor, the map, the
> source rest and `RetargetOptions`, which is the same inputs held for reuse.
>
> This document owns skeleton descriptors, retarget maps, rest correction,
> root-motion modes and retarget diagnostics. On that area it wins over
> [DESIGN_POLICY.md](DESIGN_POLICY.md) §10–§13. Section numbers are stable;
> open questions are `RT-O<n>`.

---

## 1. What a retarget is

A retarget takes a `MotionPose` in the canonical humanoid
([MOTION_CONTRACT.md](MOTION_CONTRACT.md)), a `SkeletonDescriptor`, a
`RetargetMap` and a `RetargetPolicy`, and returns the target skeleton's joint
transforms. It is **plain values in, plain values out**: it opens no stage,
reads no file and knows no avatar format (design policy §4.2). It is
deterministic for identical inputs (§12).

```cpp
RetargetedPose Retarget(const MotionPose&, const SkeletonDescriptor&,
                        const RetargetMap&, const RetargetPolicy&,
                        RetargetDiagnostics* diagnostics);
```

A realtime caller builds the descriptor, the map and the rest correction
**once** and reuses them every frame (design policy §31).

## 2. `SkeletonDescriptor`

- Joint names (source names kept verbatim, Unicode included — design policy
  §10), parent indices in which **every parent precedes its child**, and rest
  (and bind) transforms.
- Built by the format repository or by `motionUsd` from a `UsdSkelSkeleton`.
  Building one from joint tokens and **rest matrices** is a library function
  here — decomposing a matrix into rest rotation, translation and scale, with
  shear leaking into the scale as §6.1 states — because today both a CLI and an
  OpenExec bundle in `usd-vrm-plugins` carry their own copy of that
  decomposition (its OpenExec humanoid finding).
- Equality is exact and defined: a cached descriptor is compared, not trusted.

## 3. `RetargetMap`

- A `HumanJoint` drives a target joint **only through an explicit entry**.
  The retargeter never matches joint names heuristically.
- *Building* an entry heuristically is allowed, and belongs to whoever builds
  the map: `usd-mmd-plugins` maps MMD's conventional bone names with a role
  table it owns, `usd-vrm-plugins` reads the VRM humanoid binding, and a
  caller can supply a file. The retargeter cannot tell which (design policy
  §11).
- An entry is validated when the map is built, not per frame: an index the
  skeleton does not have, two joints mapped to one target, and a map that
  leaves a required joint unmapped are each a diagnostic (§7).

## 4. Missing and extra joints

- A joint the pose drives and the map does not bind is **reported**, never
  guessed.
- A target joint no entry drives keeps its **rest transform**. It never
  collapses to identity.
- A joint absent from the pose (MOTION_CONTRACT §5.2) leaves its target at the
  value the policy names: rest by default.
- Which joints a target *requires* is the map's statement, not the
  vocabulary's: every `HumanJoint` is optional in a pose.
- The seven partial-skeleton cases are §4.1. That was RT-O2, carried from
  `usd-vrm-plugins`' v0.9.0 decision.

### 4.1 Partial skeletons (carried from `usd-vrm-plugins` v0.9.0)

A clip and a rig rarely name the same joints, and retargeting across the
difference is legal and useful, so **none of these cases refuses a retarget**.
What each one costs is stated and, where it can be, reported under a §7 code.
`usd-vrm-plugins` decided this on 2026-09-17 (its OpenExec plan's P1-3), with a
named test holding every row
([its MOTION_CONTRACT.md, "Partial skeleton policy"](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/design/MOTION_CONTRACT.md#partial-skeleton-policy-v090)).
Those tests arrive with `motionRetarget`.

| # | Case | What the retarget does | Reported |
| --- | --- | --- | --- |
| 1 | a joint the clip drives and the rig does not bind | its motion reaches nothing | unbound driven joint, once per clip, including a joint first driven after the first sample |
| 2 | a joint the rig has and the clip does not drive | it stays at its **rest**, with rotation, translation and rest scale; never identity | nothing |
| 3 | a joint the map requires that the rig does not bind, or binds to an index the rig lacks | the retarget proceeds without it. For `hips` under `Hips` root motion, root motion is dropped | missing required joint, from `DiagnoseRig` before any clip; for `hips` the detail says the root was dropped |
| 4 | an optional joint missing (eyes, jaw, toes, shoulders, `upperChest`, fingers) | a rig binding every required joint and none of these is complete | nothing, unless a clip drives one, which is case 1 |
| 5 | two joints bound to one target | offline, joints are written in vocabulary order, so of the joints a sample drives the **later** one wins; the OpenExec map node **refuses** the map | duplicate target, on the target |
| 6 | the chains disagree: an intermediate joint exists on one side only, or the rig is out of parent-before-child order | each bound joint carries its motion **relative to its own parent** on each side. A missing intermediate's motion is dropped (case 1), **not folded into its child** | the dropped joint as unbound; an out-of-order rig as an invalid hierarchy |
| 7 | a parent whose rest is not identity, on either side | the correction reads each side's **accumulated** parent rest, every ancestor included, so the world delta survives | nothing |

**Where the two implementations part.** Rows 5 and 3 (for a binding to a
joint the rig lacks) are decided when the map is built, and there the OpenExec
node is stricter: a computation cannot bake a warning beside a value, so it
refuses a map the offline tool would warn about and use.

**Not promised.** Folding an unbound intermediate joint's rotation into its
child would change every bake of a clip with `upperChest` onto a rig without
one. That would be a contract change with its own parity evidence, not a fix.

Which joints are *required* is the caller's statement (§3, §4). In
`usd-vrm-plugins` it is VRM 1.0's required-bone set. Since the import the
retargeter takes the set from its caller, as `RetargetOptions::requiredBones`,
empty by default (that repository's WORKSPACE.md §9.5, finding 1). One bone
is required by the options themselves: under `Hips` root motion the root
lands on the hips, so a rig without them is reported whatever the set says.

## 5. Rest-pose correction

What survives a change of rig is a joint's **world-space rotation away from
its own rest**. With source rest `S`, source parent rest `Sp`, target rest
`T` and target parent rest `Tp`, equating the two world deltas

```text
Tp * Qt * T^-1 * Tp^-1  ==  Sp * Qs * S^-1 * Sp^-1
```

gives

```text
Qt = (Tp^-1 * Sp) * Qs * (S^-1 * Sp^-1 * Tp * T)
```

composing the OpenUSD way (`a * b` applies `b` first). Where both rests are
identity, the sample passes through unchanged. The two bracketing terms depend
only on the rests, so they are computed once per joint (§1).

A source whose rest is not the canonical identity — a BVH export, an evaluated
MMD rig — states its rest explicitly; a rest derived by a second traversal of
the source can disagree with the first and shows as a constant per-joint
offset that looks like a bad capture.

## 6. Root motion

Root motion stays separate from the body (MOTION_CONTRACT §5.3), so where it
lands is decided here, at retarget time, and never silently (design policy
§13).

**What carries is the delta.** The source hips translation minus the source
rest hips translation, added to the target's rest translation — so a clip
authored on a 1.0 m rig drives a 1.6 m one without the avatar snapping to the
source's hip height. Two stated adjustments: a uniform `translationScale`, and
`preserveTargetHeight`, which takes the horizontal delta only.

The imported implementation has three modes; the design policy names five.
**The published vocabulary is the imported one** (RT-O1, decided 2026-09-19):
`Hips`, `RootJoint` and `Ignore`, with `translationScale` and
`preserveTargetHeight`. It carries the OpenExec parity `usd-vrm-plugins`
measured, and a name with no implementation behind it would be a promise the
API cannot keep. The table maps the design policy's names onto it.

| Imported (`usd-vrm-plugins`) | Design policy §13 | Meaning |
| --- | --- | --- |
| `Hips` (default) | `Preserve` | the delta lands on the joint mapped to `hips` |
| `RootJoint` | `Extract` | the delta lands on a named root joint; the hips stay at rest |
| `Ignore` | `InPlace` / `Remove` | no translation from the clip; the body animates in place |
| — | `ProjectToGround` | not implemented |

`Remove` and `InPlace` differ in the design policy only if one keeps the
root's yaw; `preserveVerticalMotion` and `preserveYaw` are not implemented.
Each arrives with the first consumer that needs it, as a new mode or flag
beside these. A `RootJoint` request with no valid joint degrades to `Ignore`
and says so.

### 6.1 Scale (carried from `usd-vrm-plugins` v0.9.0)

RT-O3, carried from `usd-vrm-plugins`' decision of 2026-09-17 (its OpenExec
plan's P1-2;
[its MOTION_CONTRACT.md, "Scale policy"](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/design/MOTION_CONTRACT.md#scale-policy-v090)):

1. **A bake states each joint's rest scale, constant over the clip.** UsdSkel
   takes an animated joint's local transform from the animation whole, so the
   `scales` a bake authors *are* the rig's scales. Identity would replace a
   scaled rest, not keep it. Measured: a fixture arm rested at 2 baked at 1,
   and a published avatar's seven scaled secondary joints, at most 0.14% off
   unit, baked unscaled. The value is `half`, UsdSkel's type.
2. **Scale is not retargeted.** A pose carries no scale, and a clip's rest
   scale does not enter the rest correction (§5).
3. **A clip that animates scale is reported, never applied and never dropped
   silently**: once, on the animation, naming the first joint and instant. It
   is a caller's code, because the library never receives a clip's scale.
4. **A rig whose rest is scaled is not refused.** A shear leaks into the
   decomposed scale as each basis row's length. A UsdSkel rest carries none.

`SkeletonDescriptor` therefore carries a rest scale per joint (§2).

## 7. Diagnostics

A diagnostic is a **value**: a code, a subject (a joint name, a joint index, a
path) and a detail for a person. **The code and the subject are the
contract**; tests assert them, never prose.

- A retarget reports into a list holding each code-and-subject **once**, in
  the order raised: an unbound joint is one fact about a clip and a map,
  however many samples carry it. Two lists compare exactly, in order — which
  is what lets an offline tool and an OpenExec graph be proven equal.
- The rig's own report (`DiagnoseRig`) and each sample's report compose: a
  caller retargeting one pose at a time reaches the clip's list by asking once
  per rig and once per pose.
- **The set splits at the layer boundary**, and the split is checked. The
  library raises only what plain values can show; codes about a stage, a file
  or an output path are raised by the caller that holds one.

The events, with the codes they arrived under. Each imported
`VRM_RETARGET_*` code took this repository's prefix on arrival, its event name
unchanged (design policy §42.8), and the catalog is
[reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md) §2.2:

| Event | Code | Severity | Raised by |
| --- | --- | --- | --- |
| a required joint unmapped, or mapped to an index the skeleton lacks | `MOTION_RETARGET_MISSING_REQUIRED_BONE` | warning | library |
| a joint the pose drives and the map does not bind | `MOTION_RETARGET_UNBOUND_DRIVEN_BONE` | warning | library |
| two joints mapped to one target | `MOTION_RETARGET_DUPLICATE_TARGET` | warning | library |
| a parent that does not precede its child | `MOTION_RETARGET_INVALID_HIERARCHY` | warning | library |
| an invalid root joint for `RootJoint` | `MOTION_RETARGET_INVALID_ROOT_JOINT` | warning | library |
| a clip that animates scale (§6.1) | `MOTION_RETARGET_NON_UNIT_SCALE` | warning | caller |
| a one-pose clip placed at the stage start | `MOTION_RETARGET_TIME_RANGE_DERIVED` | info | caller |
| an output that would overwrite an input | `MOTION_RETARGET_OUTPUT_COLLIDES_WITH_INPUT` | error | caller |

Every one but the last is recoverable: retargeting onto a partial rig is legal
and useful.

## 8. Later scope

Design policy §12.2 applies unchanged: limb-length-aware translation,
IK-assisted hands and feet, foot locking, contacts, twist distribution, joint
limits and locomotion warping arrive only once §1–§7 are stable, and only
with avatar-format-neutral APIs.

## 9. Open questions

RT-O2 (§4.1) and RT-O3 (§6.1) were carried from `usd-vrm-plugins`' v0.9.0
decisions on 2026-09-19.

| Id | Question | Resolve by |
| --- | --- | --- |
| ~~RT-O1~~ | **Decided 2026-09-19: the imported vocabulary** (§6). Was: root-motion vocabulary: the imported `Hips` / `RootJoint` / `Ignore` plus two flags, or the design policy's five modes with `preserveVerticalMotion` and `preserveYaw` | the import of `vrmRetarget`'s generic half |
| RT-O4 | Whether `SkeletonDescriptor` carries bind transforms separately from rest, or bind is `motionUsd`'s concern only | the first consumer that needs bind in a retarget |

## 10. API owed from the OpenExec evidence

`usd-vrm-plugins`' `execVrm` wrapped the retargeter node by node and found
where a wrapper could not reach
([its boundary-consolidation findings](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/roadmap/boundary-consolidation.md#findings-from-the-exec-layer-as-they-land)).
Each was fixed on arrival, in its own change after the move
([WORKSPACE.md §3](../architecture/WORKSPACE.md#3-moving-code-in), rule 4):

- ✅ **A `SkeletonDescriptor` from joint tokens and rest matrices** (§2):
  `BuildSkeletonDescriptor`. The arithmetic half, `DecomposeRestTransform`,
  was shared since that repository's v0.9.0; the builder adds the parents and
  the two refusals its exec bundle made (a rest count that does not pair, an
  empty token). Reading the tokens and matrices off a skeleton prim is
  `motionUsd`'s reading half.
- ✅ **A source rest pose from a semantic skeleton's tokens and decomposed
  rests**: `BuildSourceRestPose`. Which joint fills which vocabulary slot, by
  joint leaf, and which is its parent, with the bundle's two refusals (no
  vocabulary bone, a bone named twice).
- **A retarget that takes the rest correction as an input.** Today the
  retargeter computes the correction in its constructor and accepts none, so a
  node recomputes per evaluation what it caches per rig edit. Measured on a
  55-joint humanoid: 21.2 µs per evaluation, 17.7 µs of it the correction,
  against 1.9 µs for the retarget alone (its retarget report §2).
- **A per-pose diagnosis without the retarget**: the counterpart of
  `DiagnoseRig`, which `Retarget` itself calls, so the rule has one
  implementation. Without it, the diagnostics node repeats the retarget, 23 µs
  a frame on a 23-joint rig and 50–60 µs on a 128-joint avatar, and a driver's
  override of the retarget is not diagnosed (its diagnostics report §6).
- **The bake authored from the library's `JointLocalTransforms`, at the time
  code it read.** Rebuilding a time code from seconds bakes frame 62 at
  `62.00000000000001` at 30 fps (its joint-transforms report §8, §10). That is
  `motionUsd`'s, and it makes "offline and OpenExec behave identically" one
  statement.
- **Which prim a stage means**: the rig, the clip and its animation are chosen
  by rules written only in a CLI, and restated by the parity harness. They are
  either stated once, in `motionUsd`, or the stage names every one of them by
  relationship, which is the direction the evaluation's relationships already
  point ([EXEC_CONTRACT.md §5.5](EXEC_CONTRACT.md#55-which-clip-drives-which-rig-and-where-its-root-lands);
  its parity report §6, §8).
