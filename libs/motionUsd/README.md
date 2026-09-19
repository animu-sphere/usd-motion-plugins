# motionUsd

`motionUsd` turns motion values into OpenUSD. At v0.1.0 that means one
direction: a `MotionClip` becomes the **standalone motion stage**
([USD_MAPPING.md §2–§5](../../docs/design/USD_MAPPING.md#2-the-standalone-motion-stage)),
standard `UsdSkel` with nothing that names a target rig. Reading a stage back
into a clip arrives with v0.2.0, beside the retargeter (USD_MAPPING.md §7).

It is a **plain static CMake library**, not a plugin: it has no
`plugInfo.json`, no file format and no OpenExec. A format plugin that authors
a motion stage calls it. It links OpenUSD's `usd`, `sdf`, `usdGeom` and
`usdSkel`, and among this repository's libraries only `motionCore`. See
[WORKSPACE.md §2](../../docs/architecture/WORKSPACE.md#2-dependency-directions)
for the edges, enforced by [`tests/check_boundaries.py`](tests/check_boundaries.py).

It arrived from `usd-vrm-plugins` on 2026-09-19 with its history. The source
was `motion_capture`'s semantic clip writer, the one writer there that authors
an avatar-independent clip. The writer was then adapted to the mapping:
`/Animation/Skeleton` and `/Animation/Body`, always 30 time codes per second,
and `customData.motion`.

## What it provides

| Header | Contents |
| --- | --- |
| `motionUsd/ClipWriter.h` | `AuthorMotionStage` (into a stage a caller holds), `WriteMotionStage` (into a file), `MotionStageOptions`, `MotionStageReport`, `MotionStageContractVersion`, `MotionStageTimeCodesPerSecond` |

```text
/Animation            Scope, the default prim; customData.motion
  /Skeleton           UsdSkelSkeleton over semantic joint paths (hips, hips/spine, ...)
  /Body               UsdSkelAnimation bound to /Animation/Skeleton;
                      identity scales; custom uniform double motion:timeCodesPerSecond = 30
```

## Rules the writer keeps

- **Absent is not rest.** A joint no sample observed is not on the skeleton. A
  joint one sample missed is authored at identity, because holding it is an
  intake policy. A root position one sample missed is held, because the rest is
  a place and not a neutral value.
- **`scales` is always authored.** Without it UsdSkel resolves every joint to
  its rest while every query still succeeds. `motionUsd_unit` checks what
  UsdSkel resolves, not only what was authored.
- **Times are the samples'.** A sample at `t` seconds is written at `t × 30`,
  snapped to a whole frame when within 1e-6 of one. Samples whose time codes do
  not increase are refused.
- **What the mapping cannot hold yet is reported.** Channels wait for USD-O4's
  attribute names and look-at targets have no place yet, so both are counted in
  `MotionStageReport` rather than dropped in silence.

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
