# Workspace contract

The binding contract for how `usd-motion-plugins` is laid out: component
identities, their kinds and directories, the dependency directions between
them and to the rest of the ecosystem, and the invariants every change keeps.
**A structural change that contradicts this document changes this document
first, in its own pull request** — never through a README, a roadmap entry or
code.

Status (2026-09-17): **contract adopted, nothing exists.** The repository holds
documentation only. Every identity below is *reserved* until the change that
creates it lands, and its row then says so. The shape follows the design
policy's §22 and the workspace discipline `usd-vrm-plugins` and
`usd-mmd-plugins` share: plain libraries apart from plugin bundles, a manifest
beside each component, and two build modes, `ost` and plain CMake.

## 1. Identities

### 1.1 Libraries

| Identity | Directory | Role | Arrives from | Status |
| --- | --- | --- | --- | --- |
| `motion-core` | `libs/motion-core/` | `HumanJoint`, `MotionPose`, `RootMotion`, `MotionChannelSet`, `SourceMetadata`, `MotionClip`, constraints ([MOTION_CONTRACT.md](../design/MOTION_CONTRACT.md)) | `usd-vrm-plugins` `motionCore`, renamed | reserved |
| `motion-sampling` | `libs/motion-sampling/` | sampling with status, interpolation, resample, filter, blend, the pose buffer | `usd-vrm-plugins` `motionRuntime` (its sampling half) | reserved |
| `motion-recording` | `libs/motion-recording/` | stream intake, `MotionRecorder`, the `motion-capture-trace` format, replay | `usd-vrm-plugins` `motionRuntime` (its capture half) | reserved |
| `motion-retarget` | `libs/motion-retarget/` | `SkeletonDescriptor`, `RetargetMap`, rest correction, root-motion policy, retarget diagnostics ([RETARGETING_POLICY.md](../design/RETARGETING_POLICY.md)) | `usd-vrm-plugins` `vrmRetarget`, its generic half | reserved |
| `motion-usd` | `libs/motion-usd/` | `MotionClip` ↔ `UsdSkelAnimation`, `SkeletonDescriptor` ↔ `UsdSkelSkeleton`, time codes, metadata ([USD_MAPPING.md](../design/USD_MAPPING.md)) | `usd-vrm-plugins` `motion_retarget`'s `StageIo` | reserved |
| `motion-source` | `libs/motion-source/` | the format-neutral recorded-source layer: source skeleton and animation, the producer-profile contract, conversion into `MotionClip` | `usd-vrm-plugins` `motionSource` | reserved |
| `motion-bvh` | `libs/motion-bvh/` | BVH syntax and extraction only (design policy §27) | `usd-vrm-plugins` `motionBvh` | reserved |

### 1.2 Bundles, tools and data

| Identity | Kind | Directory | Role | Arrives from | Status |
| --- | --- | --- | --- | --- | --- |
| `execMotion` | optional plugin bundle (`usd-exec`) | `plugins/execMotion/` | vendor-neutral OpenExec nodes, each a thin wrapper over a library call (design policy §21) | `usd-vrm-plugins` `execMotion` | reserved |
| `motion-inspect` | CLI | `tools/motion-inspect/` | reports on a motion stage or clip | new | reserved |
| `motion-convert` | CLI | `tools/motion-convert/` | a recorded source + a named profile → a motion stage | `usd-vrm-plugins` `motion_bvh_convert` | reserved |
| `motion-bvh-inspect` | CLI | `tools/motion-bvh-inspect/` | what a BVH file holds, and which profiles fit it | `usd-vrm-plugins` `motion_bvh_inspect` | reserved |
| `motion-record` | CLI | `tools/motion-record/` | a recorded trace → a motion stage | `usd-vrm-plugins` `motion_capture` | reserved |
| producer profiles | package data | `profiles/motion/` | one declarative file per producer and export preset | `usd-vrm-plugins` `profiles/motion/` | reserved |

Libraries are plain static CMake libraries. Directory names follow the design
policy's §22; the CMake target and package names are fixed with the scaffold
(WS-O1).

### 1.3 Deliberately not here

| Not here | Where it lives | Why |
| --- | --- | --- |
| VMC, mocopi, VRChat OSC, `osc`, `liveTransport`, tracker assignment and solve | `motion-connectors` | device and protocol connectivity (design policy §3.1) |
| VRMA reading, the VRM humanoid binding, expressions, look-at, `execVrm` | `usd-vrm-plugins` | VRM semantics (design policy §3.2, §26) |
| VMD reading, MMD IK and append evaluation, the MMD role table | `usd-mmd-plugins` | MMD semantics (design policy §3.2, §42.4) |
| scheduling, physics, OpenExec driving policy | `usd-avatar-runtime` | runtime orchestration (design policy §3.3) |
| a generator's model, training or inference | never in this ecosystem's core | design policy §3, §24 |

## 2. Dependency directions

### 2.1 Inside the repository

```text
motion-core ────────→ OpenUSD foundation types only (gf, tf, vt)
motion-sampling ────→ motion-core
motion-recording ───→ motion-core, motion-sampling
motion-retarget ────→ motion-core
motion-usd ─────────→ motion-core, motion-sampling, OpenUSD (usd, sdf, usdSkel)
motion-source ──────→ motion-core
motion-bvh ─────────→ motion-source
execMotion ─────────→ motion-core, motion-sampling, motion-retarget, OpenExec
tools/* ────────────→ the libraries they name, OpenUSD stage authoring
```

This is the design policy's §24 with the recorded-source pair added.
`usd-vrm-plugins`' `vrmRetarget` also depends on its runtime library today;
whether `motion-retarget` keeps that edge or loses it is WS-O2.

### 2.2 Forbidden

| Edge | Why |
| --- | --- |
| any component → `usd-vrm-plugins`, `usd-mmd-plugins`, `motion-connectors`, `usd-avatar-runtime` | the ecosystem's direction is fixed (design policy §19.3, §39) |
| `motion-core` → OpenUSD stage, Sdf, plug or file-format APIs | a value contract, usable with no stage |
| any library → OpenExec | OpenExec is an optional layer above (design policy §21) |
| any library → a network, device, ML, UI or rendering dependency | design policy §24 |
| `motion-core`, `motion-sampling` → `motion-source`, `motion-bvh` | nothing in the core knows a file format exists |
| `motion-source` → a reader; a reader → another reader | readers hang off the format-neutral layer, never the reverse |
| `motion-bvh` → a producer name in code, or a default profile | producer semantics are declarative data |
| a component → a sibling's source tree | siblings are consumed as installed packages |

### 2.3 The ecosystem

```text
motion-connectors ─→ usd-motion-plugins ←─ usd-vrm-plugins
                              ↑
                       usd-mmd-plugins
usd-avatar-runtime ─→ all of the above
```

Consumers reach this repository only through installed packages. The
consumers planned so far, and the one component each links:

| Consumer | Links | Planned in |
| --- | --- | --- |
| `usd-vrm-plugins` | every library, as its identities move out | its WORKSPACE.md §9 and migration track |
| `usd-mmd-plugins` | `motion-core` (and `motion-retarget` for map validation) through `mmdMotionAdapter` | its WORKSPACE.md §2.4, Phase 9 |
| `motion-connectors` | `motion-core`, `motion-recording` | not yet written |
| `usd-avatar-runtime` | any, and `execMotion` | not yet written |

### 2.4 Enforcement

Gates, added with the code they guard, as in the sibling repositories: every
edge declared in the component's manifest and validated by
`ost plugin test --workspace --graph-only`; a link-line check per library
(`motion-core` links no OpenUSD beyond its foundation types); and an include
scan refusing forbidden headers and product names.

## 3. Moving code in

Code arrives from `usd-vrm-plugins` under that repository's moving rules
(its WORKSPACE.md §9.2), which this repository keeps from the receiving side:

1. **History comes with the code.**
2. **Renamed on arrival**, once
   ([DESIGN_POLICY.md §42.2](../design/DESIGN_POLICY.md#422-names-are-this-policys-applied-on-arrival)).
3. **Tests and fixtures come with it**, and the parity evidence named for the
   move is reproduced against this repository's package before the sender
   deletes its copy.
4. **The API findings are fixed on arrival**
   ([MOTION_CONTRACT.md §8](../design/MOTION_CONTRACT.md#8-motionclip-and-sampling),
   [RETARGETING_POLICY.md §2](../design/RETARGETING_POLICY.md#2-skeletondescriptor),
   [USD_MAPPING.md §7](../design/USD_MAPPING.md#7-reading-usd-back)), in a
   change of their own after the move, so a move stays a move.
5. **Nothing VRM-specific arrives.** A header that names VRM vocabulary or a
   VRM-only concept is a boundary defect and stays behind.

## 4. Versioning and build

- One `VERSION` at the root, mirrored by the tag, the changelog and every
  manifest.
- OpenUSD is pinned exactly, to the release the ecosystem pins
  ([DEPENDENCIES.md §1](DEPENDENCIES.md#1-openusd)).
- The motion contract, the joint vocabulary and the USD mapping carry their
  own versions, separate from the package version
  ([USD_MAPPING.md §8](../design/USD_MAPPING.md#8-versioning)).
- Both build modes, `ost` and plain CMake, are kept working, and every
  package is consumed from a clean installed prefix in CI.

## 5. Invariants

1. The edges are §2's, declared in manifests and gated in CI.
2. No component depends on a consumer.
3. No product, device or avatar-format name controls behaviour.
4. Every library builds and tests without OpenExec and without a renderer.
5. A capability is claimed only with a test behind it
   ([CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md)).
6. The design policy's §39 architectural invariants hold.

## 6. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| WS-O1 | CMake target and package names: kebab-case as the directories (`motion-core`), or lower-camel as the siblings' identities (`motionCore`), and the `openstrata::` target namespace | the scaffold |
| WS-O2 | Whether `motion-retarget` depends on `motion-sampling`, as `vrmRetarget` depends on its runtime library today, or on `motion-core` alone as design policy §24 says | the import of `vrmRetarget`'s generic half |
| WS-O3 | Design policy §26 sketches `plugins/motion-bvh/`; the imported BVH reader is a plain library and registers nothing. A BVH `SdfFileFormat` would be a separate, thin bundle, created only if opening `.bvh` directly is wanted | a consumer that wants it |
