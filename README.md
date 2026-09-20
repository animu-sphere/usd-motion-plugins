# OpenUSD Motion Plugins

Vendor-neutral, avatar-format-neutral motion for OpenUSD: one representation
of body motion, and the sampling, retargeting, recording and `UsdSkelAnimation`
bridge every avatar format and every motion source share.

> **Status: the core, the runtime, both halves of the stage bridge and the
> recorded-source layer have arrived.** The design policy is accepted, the contracts are
> proposed, and six libraries are imported: `motionCore` — the value types
> every other library builds on — `motionSampling`, `motionRecording`,
> `motionUsd`, `motionSource` and `motionBvh`, with `motion_convert`,
> `motion_bvh_inspect`, `motion_record`, the producer profiles and the
> `execMotion` bundle. The rest of the implementation arrives from
> [`usd-vrm-plugins`](https://github.com/animu-sphere/usd-vrm-plugins), where
> it is built and measured today, one identity at a time
> ([roadmap](docs/roadmap/current.md)); how to build the tree is in
> [docs/guides/building.md](docs/guides/building.md). The
> [capability matrix](docs/reference/CAPABILITY_MATRIX.md) is the only page
> that says what is implemented here.

## The central rule

> **Motion is represented independently of its transport, source product,
> avatar format, and runtime host.**

```text
files (BVH, …) ─┐        devices and protocols ─→ motion-connectors ─┐
                 │                                                   │
VRMA ─→ usd-vrm-plugins ─┐                                           │
VMD ─→ usd-mmd-plugins ──┼─→ MotionClip / MotionPose ←────────────────┘
                         │          │
                         │   sampling · retarget · recording · UsdSkelAnimation
                         │          │                  (this repository)
                         └──────────┴─→ usd-avatar-runtime
```

This repository is not a second VRM repository, a second MMD repository, or a
collection of device adapters. VRM and VRMA stay in `usd-vrm-plugins`, PMX and
VMD in `usd-mmd-plugins`, devices and protocols in `motion-connectors`.
Every one of them depends on this repository, and it depends on none of them.

## Planned components

| Component | Role | Release |
| --- | --- | --- |
| `motionCore` | `HumanJoint`, `MotionPose`, `RootMotion`, channels, provenance, `MotionClip` | v0.1.0 |
| `motionSampling` | sampling with status, interpolation, filtering, blending | v0.1.0 |
| `motionRecording` | stream intake, recorder, the capture trace format | v0.1.0 |
| `motionUsd` | motion ↔ `UsdSkelAnimation` and `UsdSkelSkeleton` (both halves, 2026-09-20) | v0.1.0, v0.2.0 |
| `motionRetarget` | skeleton descriptors, maps, rest-pose correction, root-motion policy (imported 2026-09-19) | v0.2.0 |
| `motionSource`, `motionBvh` | recorded sources through declarative producer profiles | v0.4.0 |
| `execMotion` | optional OpenExec nodes over the same libraries (imported 2026-09-20) | v0.5.0 |

Identities and dependency directions are fixed in
[docs/architecture/WORKSPACE.md](docs/architecture/WORKSPACE.md); which release
carries what, in the [roadmap](docs/roadmap/README.md#status-at-a-glance).

## Canonical conventions

Right-handed, +Y up, +Z forward, metres, seconds; local joint rotations
relative to the semantic parent; root motion separate from the hips; a missing
joint is valid state, never an identity rotation
([MOTION_CONTRACT.md](docs/design/MOTION_CONTRACT.md)).

## Documentation

| | |
| --- | --- |
| [docs/design/](docs/design/) | The design policy and the motion, retargeting and USD contracts |
| [docs/architecture/](docs/architecture/) | The workspace contract and external dependencies |
| [docs/reference/](docs/reference/) | What is implemented, and diagnostics |
| [docs/roadmap/](docs/roadmap/) | What comes next |
| [docs/contributing/](docs/contributing/) | How the documentation is maintained |

Changes are recorded in the [changelog](CHANGELOG.md).

## License

Apache-2.0 — see [LICENSE](LICENSE).
