# motionUsd

`motionUsd` turns motion values into OpenUSD and back. A `MotionClip` becomes
the **standalone motion stage**
([USD_MAPPING.md §2–§5](../../docs/design/USD_MAPPING.md#2-the-standalone-motion-stage)),
standard `UsdSkel` with nothing that names a target rig, and a stage becomes a
`MotionClip` again ([§7](../../docs/design/USD_MAPPING.md#7-reading-usd-back)).
Baking a retargeted clip onto a target rig (§6) is not here.

It is a **plain static CMake library**, not a plugin: it has no
`plugInfo.json`, no file format and no OpenExec. A format plugin that authors
a motion stage calls it. It links OpenUSD's `usd`, `sdf`, `usdGeom` and
`usdSkel`, and among this repository's libraries only `motionCore`. See
[WORKSPACE.md §2](../../docs/architecture/WORKSPACE.md#2-dependency-directions)
for the edges, enforced by [`tests/check_boundaries.py`](tests/check_boundaries.py).

Both halves arrived from `usd-vrm-plugins` with their history. The writer came
on 2026-09-19 from `motion_capture`'s semantic clip writer, the one writer
there that authors an avatar-independent clip; the reader on 2026-09-20 from
`motion_retarget`'s `StageIo`, whose bake onto a VRM avatar stayed behind.
Each was adapted to the mapping after its move.

## What it provides

| Header | Contents |
| --- | --- |
| `motionUsd/MotionStage.h` | `MotionStageContractVersion`, `MotionStageTimeCodesPerSecond` — what both halves state about the stage |
| `motionUsd/ClipWriter.h` | `AuthorMotionStage` (into a stage a caller holds), `WriteMotionStage` (into a file), `MotionStageOptions` (with a producer's `MotionStageRest` and provenance), `MotionStageReport` |
| `motionUsd/ClipReader.h` | `ReadMotionStage` (from a stage a caller holds), `OpenMotionStage` (from a file), `PoseFromStageSample` (values only, no stage), `MotionStageRead` with its `MotionStageSkeleton` and `MotionStageMetadata` |

```text
/Animation            Scope, the default prim; customData.motion, customData.source
  /Skeleton           UsdSkelSkeleton over semantic joint paths (hips, hips/spine, ...)
  /Body               UsdSkelAnimation bound to /Animation/Skeleton;
                      identity scales; custom uniform double motion:timeCodesPerSecond = 30
  /Channels           one typeless prim per channel:
                      uniform string motion:channelName, float motion:channelValue
```

## Rules the writer keeps

- **Absent is not rest.** Without a producer rest, a joint no sample observed
  is not on the skeleton. A joint one sample missed is authored at its rest
  rotation, because holding it is an intake policy. A root position one sample
  missed is held, because the rest is a place and not a neutral value.
- **A producer's rest is the rig.** A recorded file states a rest, and
  `MotionStageOptions::rest` carries it. Its joints are then the joint set,
  and every joint holds its rest translation. A capture passes none, and its
  rest is identity except the hips at the first root position
  (USD_MAPPING.md §3). `motion_convert` is the caller that passes one.
- **A refusal touches nothing.** `AuthorMotionStage` checks the clip before it
  authors, and refuses a stage that already holds `/Animation`.
  `WriteMotionStage` writes the file only after the clip is accepted.
- **`scales` is always authored.** Without it UsdSkel resolves every joint to
  its rest while every query still succeeds. `motionUsd_unit` checks what
  UsdSkel resolves, not only what was authored.
- **Times are the samples'.** A sample at `t` seconds is written at `t × 30`,
  snapped to a whole frame when within 1e-6 of one. Samples whose time codes do
  not increase are refused.
- **A channel's key is its name, not its path.** The writer sanitizes the
  semantic into a prim name and refuses two semantics that collide there; the
  reader keys on `motion:channelName` and never on the path (USD §4.3). A
  channel is read back only where the stage keyed it, because USD holds the
  last key forward and a held value is not one the producer reported.
- **What the mapping still cannot hold is reported.** A look-at target has no
  place in it, so `MotionStageReport` counts the samples that carried one
  rather than dropping them in silence.

## Rules the reader keeps

- **A semantic skeleton reads; any other is a retarget.** A skeleton no joint
  token of which names the vocabulary is refused, not guessed at (USD §7).
- **The skeleton comes back as values.** `jointTokens` and `restTransforms`
  are what `motionRetarget`'s `BuildSkeletonDescriptor` and
  `BuildSourceRestPose` take, so reading a stage links no retargeter.
- **The hips are read twice**, which is the contract's rule: their rotation is
  `root.worldOrientation` as well as the local rotation
  (MOTION_CONTRACT.md §5.3).
- **`PoseFromStageSample` takes values, not a prim**, so a caller holding a
  stage and an OpenExec node holding already-resolved inputs apply one rule.
- **A warning is not a refusal.** A stage that states no rest transforms, two
  rates that disagree, a contract version from the future or a channel twice
  is read, and says so.

## Building

It builds as part of the repository root `CMakeLists.txt`. Standalone:

```sh
cmake -S libs/motionUsd -B build/motion-usd \
      -DCMAKE_PREFIX_PATH="<usd-install>;<motionCore-install>"
cmake --build build/motion-usd --config Release
ctest --test-dir build/motion-usd -C Release --output-on-failure
```

Consumers use the installed package contract:

```cmake
find_package(motionUsd CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE motionUsd::motionUsd)
```
