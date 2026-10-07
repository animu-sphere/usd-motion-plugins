# motionUsd

`motionUsd` turns motion values into OpenUSD and back. A `MotionClip` becomes
the **standalone motion stage**
([USD_MAPPING.md §2–§5](../../docs/design/USD_MAPPING.md#2-the-standalone-motion-stage)),
standard `UsdSkel` with nothing that names a target rig, and a stage becomes a
`MotionClip` again ([§7](../../docs/design/USD_MAPPING.md#7-reading-usd-back)).
Already evaluated joint-local samples can also be baked onto a composed
target rig ([§6](../../docs/design/USD_MAPPING.md#6-motion-on-an-avatar)).

It is a **plain static CMake library**, not a plugin: it has no
`plugInfo.json`, no file format and no OpenExec. A format plugin that authors
a motion stage calls it. It links OpenUSD's `usd`, `sdf`, `usdGeom` and
`usdSkel`, and among this repository's libraries only `motionCore`. See
[WORKSPACE.md §2](../../docs/architecture/WORKSPACE.md#2-dependency-directions)
for the edges, enforced by [`tests/check_boundaries.py`](tests/check_boundaries.py).

## What it provides

| Header | Contents |
| --- | --- |
| `motionUsd/MotionStage.h` | `MotionStageContractVersion`, `MotionStageTimeCodesPerSecond` — what both halves state about the stage |
| `motionUsd/ClipWriter.h` | `AuthorMotionStage` (into a stage a caller holds), `WriteMotionStage` (into a file), `MotionStageOptions` (with a producer's `MotionStageRest` and provenance), `MotionStageReport` |
| `motionUsd/ClipReader.h` | `ReadMotionStage` (from a stage a caller holds), `OpenMotionStage` (from a file), `MotionStageReadOptions` (explicit owner-selected scalar/gaze inputs), `PoseFromStageSample` (joint values only, no stage), `MotionStageRead` with its `MotionStageSkeleton` and `MotionStageMetadata` |
| `motionUsd/SkeletonAnimationWriter.h` | `AuthorSkeletonAnimation`, `SkeletonAnimationSample`: target-local arrays into a new animation and a skeleton binding override |
| `motionUsd/SkeletonReader.h` | `ReadSkeleton`: strict owned metre rest/parent/placement arrays; `ReadMotionSkeleton`: owner-built descriptor and explicitly selected semantic source rest with metadata; `ReadCanonicalMotionStage`: coherent clip/descriptor/source-rest result with canonical source-space checks; owner code/subject/detail refusals |

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
- **Gaze preserves presence.** `Body.motion:lookAtTarget` carries canonical
  clip-space points only at reported keys, including origin targets.
  `MotionStageReport::unauthoredLookAtTargets` is zero on successful writes.

## Rules the reader keeps

- **Supplementary input is explicit.** Common gaze joins the body/channel
  time union, with exact-key presence or a default applying to every pose.
  `MotionStageReadOptions` lets a format owner select scalar name/value and
  gaze attribute paths. Names come from authored string/token values, weights
  remain unclamped, and the host explicitly places gaze in runtime-world space.
  Native format fields are not automatically discovered. See
  [USD §4.4–§4.5](../../docs/design/USD_MAPPING.md#44-gaze-points) for space,
  unsupported intake, error and owner boundaries.

- **A semantic skeleton reads; any other is a retarget.** A skeleton no joint
  token of which names the vocabulary is refused, not guessed at (USD §7).
- **The strict reader builds the rest.** `ReadMotionSkeleton` explicitly
  selects generic or semantic-source interpretation and invokes the existing
  `motionRetarget` builders/validators. `ReadCanonicalMotionStage` fills the
  additive descriptor/source-rest fields together; the permissive reader keeps
  compatible raw arrays and leaves those fields unset.
- **The producer's rate is not the stage's.** `timeCodesPerSecond` says where
  the samples were written (the writer uses 30); `customData.motion.nominalFrameRate`
  says what they were taken at, and that is what the clip comes back with.
- **Times must survive conversion.** The complete input union must produce
  finite increasing seconds with finite adjacent spans. Negative keys are valid;
  playback bounds do not crop keys. Unusable rates have documented fallback or
  strict refusal policies; see [USD §7.3](../../docs/design/USD_MAPPING.md#73-reader-time-code-policy).
- **The hips are read twice**, which is the contract's rule: their rotation is
  `root.worldOrientation` as well as the local rotation
  (MOTION_CONTRACT.md §5.3).
- **`PoseFromStageSample` takes values, not a prim**, so a caller holding a
  stage and an OpenExec node holding already-resolved inputs apply one rule.
- **A warning is not a refusal.** A stage that states no rest transforms, two
  rates that disagree, a contract version from the future or a channel twice
  is read, and says so.

The additive strict readers are a scoped consolidation boundary, not a change
to permissive `ReadMotionStage`. `ReadSkeleton` selects an absolute prim path,
requires Y-up/authored parent-local rest and returns owned `MotionStageSkeleton`
arrays, parent indices and separate rigid world placement. It validates unique
relative joint paths/topology, finite positive float-representable TRS, shear,
reflection and bounded affine residuals, converts rest/placement translations
to metres and leaves stage content untouched. `ReadCanonicalMotionStage` adds
metre units, identity placement and finite positive encoding rate before calling
the existing clip reader; its options overload passes explicit owner-selected
scalar/gaze inputs through that same strict profile. Metadata and warnings are
preserved. Refusals leave
the destination unchanged and retain a motion-owned code/subject/detail.

WS-O4's public `motionUsd` → `motionRetarget` dependency provides typed results
([WORKSPACE §2.5](../../docs/architecture/WORKSPACE.md#25-runtime-boundary-target));
the manifest, public links, installed config and boundary gates implement this
edge. `ReadMotionSkeleton` returns a descriptor with optional semantic source
rest, source units and separate rigid placement. Unsupported semantic sources
are refused rather than assigned identity rests; generic rigs require no human
roles. `motionUsd_skeletonReader` covers referenced stages, hierarchy/rest parity,
numeric/profile/ownership/refusal behavior. Installed consumers verify discovery
and linking of the public retarget dependency. Runtime adoption remains in
[Runtime Boundary Phases 2-3](../../docs/roadmap/runtime-boundary.md).

## Rules the target animation writer keeps

- The caller composes the avatar by reference, supplies evaluated rotations
  and translations in its skeleton's exact joint order, and saves the
  derivative afterwards. `motionUsd` does not retarget or interpret a format.
- Translations are in the target stage's units and basis. The caller converts
  canonical metre values before calling when the target uses another unit.
- The root layer receives a new `UsdSkelAnimation` and a
  `skel:animationSource` override. Referenced layers, rig attributes, stage
  metadata and the caller's edit target are preserved.
- Every joint receives its constant rest scale. Identity would replace a
  scaled rest; omitting scales would make UsdSkel resolve no animation.
- The stage uses 30 time codes per second, and sample times are encoded with
  the same frame snap as the standalone writer. This writer leaves scene
  metadata, including the playback interval, to the caller.
- Invalid samples, a mismatched joint order, malformed rests, an existing
  animation path, and instance/prototype authoring are refused before mutation.
  An authoring failure or a stronger binding opinion restores root content.
  `motionUsd_skeletonAnimation` checks serialized reference composition,
  resolved local transforms, scale preservation, refusal and rollback.

## Building

It builds as part of the repository root `CMakeLists.txt`. Standalone:

```sh
cmake -S libs/motionUsd -B build/motion-usd \
      -DCMAKE_PREFIX_PATH="<usd-install>;<motionCore-install>;<motionRetarget-install>"
cmake --build build/motion-usd --config Release
ctest --test-dir build/motion-usd -C Release --output-on-failure
```

Consumers use the installed package contract:

```cmake
find_package(motionUsd CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE motionUsd::motionUsd)
```
