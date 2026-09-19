# Retargeting policy

> Status: **proposed**, 2026-09-17. Not implemented in this repository. The
> generic half of `usd-vrm-plugins`' `vrmRetarget` — the pose retargeter, rest
> correction and root-motion policy — implements §3, §5 and §6 today, with
> hand-authored fixtures and OpenExec / offline parity, and arrives here as
> `motionRetarget` ([DESIGN_POLICY.md §42.1](DESIGN_POLICY.md#421-the-core-is-imported-from-usd-vrm-plugins-not-rewritten)).
> Its VRM half — the humanoid map read from `VrmHumanoidAPI`, expression and
> look-at resolution — stays in `usd-vrm-plugins`.
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
  here — decomposing a matrix into rest rotation and translation, dropping
  scale and shear with a stated rule — because today both a CLI and an
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
- The seven partial-skeleton cases `usd-vrm-plugins` has listed and not yet
  decided (its OpenExec plan's P1-3) are RT-O2.

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

| Imported (`usd-vrm-plugins`) | Design policy §13 | Meaning |
| --- | --- | --- |
| `Hips` (default) | `Preserve` | the delta lands on the joint mapped to `hips` |
| `RootJoint` | `Extract` | the delta lands on a named root joint; the hips stay at rest |
| `Ignore` | `InPlace` / `Remove` | no translation from the clip; the body animates in place |
| — | `ProjectToGround` | not implemented |

`Remove` and `InPlace` differ in the design policy only if one keeps the
root's yaw; `preserveVerticalMotion` and `preserveYaw` are not implemented.
Which vocabulary the published API uses is RT-O1. A `RootJoint` request with
no valid joint degrades to `Ignore` and says so.

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

The imported events, with their `usd-vrm-plugins` codes. Their codes here
follow [reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md) once DIAG-O1 is
resolved:

| Event | Imported code | Severity | Raised by |
| --- | --- | --- | --- |
| a required joint unmapped, or mapped to an index the skeleton lacks | `VRM_RETARGET_MISSING_REQUIRED_BONE` | warning | library |
| a joint the pose drives and the map does not bind | `VRM_RETARGET_UNBOUND_DRIVEN_BONE` | warning | library |
| two joints mapped to one target | `VRM_RETARGET_DUPLICATE_TARGET` | warning | library |
| a parent that does not precede its child | `VRM_RETARGET_INVALID_HIERARCHY` | warning | library |
| an invalid root joint for `RootJoint` | `VRM_RETARGET_INVALID_ROOT_JOINT` | warning | library |
| a non-unit rest scale (scale policy undecided) | `VRM_RETARGET_NON_UNIT_SCALE` | warning | caller |
| a one-pose clip placed at the stage start | `VRM_RETARGET_TIME_RANGE_DERIVED` | info | caller |
| an output that would overwrite an input | `VRM_RETARGET_OUTPUT_COLLIDES_WITH_INPUT` | error | caller |

Every one but the last is recoverable: retargeting onto a partial rig is legal
and useful.

## 8. Later scope

Design policy §12.2 applies unchanged: limb-length-aware translation,
IK-assisted hands and feet, foot locking, contacts, twist distribution, joint
limits and locomotion warping arrive only once §1–§7 are stable, and only
with avatar-format-neutral APIs.

## 9. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| RT-O1 | Root-motion vocabulary: the imported `Hips` / `RootJoint` / `Ignore` plus two flags, or the design policy's five modes with `preserveVerticalMotion` and `preserveYaw` | the import of `vrmRetarget`'s generic half |
| RT-O2 | Partial skeletons: the seven cases `usd-vrm-plugins` listed (its P1-3) as a contract | that task, then carried here |
| RT-O3 | A rig whose rest is scaled: carry the rest scale, refuse the rig, or keep identity and state the cost (measured on one model: seven non-humanoid joints, at most 0.14% off unit) | `usd-vrm-plugins`' P1-2 decision, then carried here |
| RT-O4 | Whether `SkeletonDescriptor` carries bind transforms separately from rest, or bind is `motionUsd`'s concern only | the first consumer that needs bind in a retarget |
