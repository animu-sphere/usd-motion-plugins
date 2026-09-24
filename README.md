# OpenUSD Motion Plugins

[![CI](https://github.com/animu-sphere/usd-motion-plugins/actions/workflows/ost-source-ci.yml/badge.svg?event=pull_request)](https://github.com/animu-sphere/usd-motion-plugins/actions/workflows/ost-source-ci.yml)
[![OpenUSD 26.08](https://img.shields.io/badge/OpenUSD-26.08-2f6f9f)](docs/architecture/DEPENDENCIES.md#1-openusd)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache--2.0-4b8bbe.svg)](LICENSE)

An OpenUSD-oriented motion interoperability and processing layer for
representing, transforming, retargeting, recording, and bridging motion data.

## Scope

**`usd-motion-plugins` is the canonical owner of generic motion semantics.**
It defines `MotionPose`, `MotionClip`, `MotionStream` and the `HumanJoint`
vocabulary, root motion, sampling, filtering, recording, generic retargeting,
generic motion files, the OpenUSD motion mapping, and OpenExec motion
evaluation. Sibling repositories consume these contracts and do not redefine
them.

It does not own:

| Responsibility | Owner |
| --- | --- |
| Device and protocol acquisition | [`motion-connectors`](https://github.com/animu-sphere/motion-connectors) |
| VRM or MMD avatar semantics | [`usd-vrm-plugins`](https://github.com/animu-sphere/usd-vrm-plugins), `usd-mmd-plugins` |
| Physical simulation | `usd-physics-plugins` |
| Execution and update loops | `usd-stage-runner` |
| Runtime composition | `usd-avatar-runtime` |

## Architecture

```text
external or canonical motion
    |
    v
representation -> canonicalization -> transformation -> retargeting
    |                                      |
    +---------- recording / evaluation ---+
    |
    v
OpenUSD interoperability -> reusable motion data
```

Inputs arrive already acquired or decoded. Format-specific adapters remain in
their owning repositories; generic motion containers are converted to the
canonical model before downstream processing.

## Components

| Component | Responsibility |
| --- | --- |
| `motionCore` | The canonical values: `HumanJoint`, `MotionPose`, `RootMotion`, `MotionChannelSet`, `SourceMetadata`, `MotionClip`, basis conversion |
| `motionSampling` | Sampling with status, interpolation, resampling, filtering, blending |
| `motionRecording` | Stream intake, recording, the `motion-capture-trace` format, replay |
| `motionRetarget` | Skeleton descriptors, retarget maps, rest-pose correction, root-motion policy, retarget diagnostics |
| `motionUsd` | `MotionClip` ↔ `UsdSkelAnimation`, and reading a skeleton as values |
| `motionSource` | Recorded sources and the producer-profile contract |
| `motionBvh` | BVH syntax and extraction |
| `execMotion` | Optional OpenExec nodes, each a wrapper over one library call |
| `motion_convert`, `motion_bvh_inspect`, `motion_record` | CLIs: a recorded source to a motion stage, what a BVH holds, a trace to a motion stage |

Identities and dependency directions:
[docs/architecture/WORKSPACE.md](docs/architecture/WORKSPACE.md).

## Documentation

| | |
| --- | --- |
| [What is implemented](docs/reference/CAPABILITY_MATRIX.md) | The capability matrix, the only source for implementation status |
| [Incomplete work](docs/roadmap/current.md) | What remains, and the open decisions |
| [Release history](CHANGELOG.md) | The changelog, and the per-version [release records](docs/releases/) |
| [docs/](docs/README.md) | Which document owns which subject |

## Build

[docs/guides/building.md](docs/guides/building.md) builds and tests the tree
with `ost` or with plain CMake against an OpenUSD 26.08 install.

## License

Apache-2.0 — see [LICENSE](LICENSE).
