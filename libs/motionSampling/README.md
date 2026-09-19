# motionSampling

`motionSampling` answers *what is the pose at this time?* over the
`motionCore` values, and says how it answered: a bounded pose history,
interpolation, resampling, smoothing and blending, and the sampling interface
that reports a status beside every pose
([MOTION_CONTRACT.md §8](../../docs/design/MOTION_CONTRACT.md#8-motionclip-and-sampling)).

It is a **plain static CMake library**, not a plugin bundle. It has no
`plugInfo.json`, no `openstrata.plugin.yaml`, no USD stage, `Sdf`, `Plug` or
file-format dependency, and no transport — only OpenUSD's `Gf` value types,
inherited through `motionCore`. See
[WORKSPACE.md §2](../../docs/architecture/WORKSPACE.md#2-dependency-directions)
for the edges, enforced by [`tests/check_boundaries.py`](tests/check_boundaries.py).

It arrived from `usd-vrm-plugins` on 2026-09-19 with its history, as the
sampling half of that repository's `motionRuntime`; the capture half is
[`motionRecording`](../motionRecording/README.md). Both were renamed on
arrival ([DESIGN_POLICY.md §42.2](../../docs/design/DESIGN_POLICY.md#422-names-are-this-policys-applied-on-arrival)).

## What it provides

| Header | Contents |
| --- | --- |
| `motionSampling/PoseBuffer.h` | `PoseBuffer` — bounded, strictly ordered pose history with bracketed sampling and capped position extrapolation |
| `motionSampling/Interpolation.h` | `SlerpShortest`, `LerpRootMotion`, `LerpPose` |
| `motionSampling/Resample.h` | `Resample`, `SampleAnimation` (the pose `SampleClip` answers, without the status) |
| `motionSampling/Filter.h` | `PoseFilter` — frame-rate independent exponential smoothing; `PoseFilter::Step`, the same step as a pure function that returns the state beside the pose |
| `motionSampling/Blend.h` | `BlendPoses`: two-pose, and weighted N-pose, which answers nullopt when there is nothing to blend |
| `motionSampling/MotionSource.h` | `IMotionSource`, `PoseSampleResult` / `PoseSampleStatus` (with an exact `operator==`, so OpenExec can register the result), `SampleClip`, `ClipSource` |

The API findings `usd-vrm-plugins`' OpenExec layer measured against this code
were fixed in a change of their own after the move (MOTION_CONTRACT.md §8).
Each fix is a pure function, and the streaming class beside it calls it:
`SampleClip`, `PoseFilter::Step` and the N-way blend's *nothing to blend*.

## Two rules the whole library obeys

- **A missing sample is not a zero sample.** Every operation preserves
  `MotionPose::validRotations`, the `RootMotion` presence flags and the channel
  names each pose reported. Where one input carries a joint or a channel and
  the other does not, the value is *held*, never faded toward identity or zero.
- **Orientations stay unit quaternions and take the short arc.** `q` and `-q`
  are the same rotation, so every interpolation picks the representative on the
  near hemisphere first. N-pose blending folds inputs in pairwise for the same
  reason — a component-wise weighted sum of quaternions is not a rotation.

## Building

It builds as part of the repository root `CMakeLists.txt`. Standalone:

```sh
cmake -S libs/motionSampling -B build/motion-sampling \
      -DCMAKE_PREFIX_PATH="<usd-install>;<motionCore-install>"
cmake --build build/motion-sampling --config Release
ctest --test-dir build/motion-sampling -C Release --output-on-failure
```

Consumers use the installed package contract:

```cmake
find_package(motionSampling CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE motionSampling::motionSampling)
```
