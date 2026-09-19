# motionRetarget

`motionRetarget` expands a clip of canonical `MotionPose`s into one target
skeleton's joint order. It corrects for the two rigs' different rest poses,
decides where root motion lands, and reports what the rig and the clip did not
say as diagnostic values
([RETARGETING_POLICY.md](../../docs/design/RETARGETING_POLICY.md)).

It is a **plain static CMake library**, not a plugin bundle. It has no
`plugInfo.json`, no `openstrata.plugin.yaml`, no USD stage, `Sdf`, `Plug` or
file-format dependency, and no OpenExec: only OpenUSD's `Gf` value types. See
[WORKSPACE.md §2](../../docs/architecture/WORKSPACE.md#2-dependency-directions)
for the edges, enforced by [`tests/check_boundaries.py`](tests/check_boundaries.py).

It arrived from `usd-vrm-plugins` on 2026-09-19 with its history, as the
generic half of that repository's `vrmRetarget`, cut by header along the line
its WORKSPACE.md §9.5 draws. The VRM half stayed there: expression resolution,
a VRM rig's look-at, and VRM 1.0's required-bone set. The types were renamed
on arrival ([DESIGN_POLICY.md §42.2](../../docs/design/DESIGN_POLICY.md#422-names-are-this-policys-applied-on-arrival)).

## It never opens a stage

The target rig arrives as plain values: a `SkeletonDescriptor`, a
`RetargetMap` and a `SourceRestPose`, not a `UsdSkelSkeleton`. Reading them off
a stage is the caller's job. A format repository does it from its own binding,
and `motionUsd` does it from a skeleton. That keeps the retarget testable
without USD composition, and usable by a live source that has no stage at all.

## What it provides

| Header | Contents |
| --- | --- |
| `motionRetarget/SkeletonDescriptor.h` | `SkeletonJoint`, `SkeletonDescriptor`: joint tokens, parents derived from `a/b/c` joint paths, decomposed rest transforms with their scale, and `DecomposeRestTransform` |
| `motionRetarget/RetargetMap.h` | `RetargetMap`: human bone → target joint index, and duplicate-binding reporting |
| `motionRetarget/RestPose.h` | `SourceRestPose`, `RestPoseCorrection`, `ComputeRestPoseCorrection` |
| `motionRetarget/RootMotionPolicy.h` | `RootMotionMode` (`Ignore` / `Hips` / `RootJoint`), `RootMotionOptions`, `ResolveRootTranslation` |
| `motionRetarget/PoseRetargeter.h` | `PoseRetargeter`, `RetargetedPose`, `RetargetedAnimation`, `JointLocalTransforms` (one retargeted sample in a `UsdSkelAnimation`'s shape, scales included), `GetJointWorldTransform`, `DiagnoseRig` |
| `motionRetarget/Diagnostics.h` | the eight `MOTION_RETARGET_*` codes (`RetargetDiagnosticCode`) and their table, `RetargetDiagnostic`, `RetargetDiagnostics`. The library raises five, and only a caller holding a stage can raise the other three |

## Four decisions worth knowing

- **Joint names are never guessed.** A binding comes from the caller: a
  format's humanoid binding, a role table, or a map file. Name heuristics are
  exactly the silent mis-retarget this contract exists to prevent, so a bone
  the caller did not bind stays unmapped and is *reported*, not inferred
  (RETARGETING_POLICY.md §3).
- **Rest-pose correction preserves the world delta.** With source rest `S`,
  source parent rest `Sp`, target rest `T` and target parent rest `Tp`, what
  survives the change of rig is the bone's world rotation away from its own
  rest. The closed form comes from equating the two deltas, and a unit test
  checks the invariant directly rather than the formula (§5).
- **Root motion carries a delta, not a height.** The hips translation is
  applied relative to each rig's own rest translation, so a clip authored on a
  1.0 m rig drives a 1.6 m one without the avatar snapping to the source's hip
  height. `preserveTargetHeight` drops the vertical component entirely (§6).
- **A diagnostic is a value.** The code and the subject are the contract, each
  pair is reported once, and two lists compare exactly and in order. That is
  what let an offline tool and an OpenExec graph be proven equal in
  `usd-vrm-plugins` (§7).

Unmapped joints keep their rest transform, so a clip that drives only part of a
rig leaves the rest of it alone instead of collapsing it to identity.

## Building

It builds as part of the workspace root `CMakeLists.txt`. Standalone:

```bash
cmake -S libs/motionRetarget -B build/motionRetarget \
      -DCMAKE_PREFIX_PATH="<usd-install>;<motionCore-install>"
cmake --build build/motionRetarget
ctest --test-dir build/motionRetarget --output-on-failure
```
