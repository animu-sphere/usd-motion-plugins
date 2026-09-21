# Workspace contract

The binding contract for how `usd-motion-plugins` is laid out: component
identities, their kinds and directories, the dependency directions between
them and to the rest of the ecosystem, and the invariants every change keeps.
**A structural change that contradicts this document changes this document
first, in its own pull request** — never through a README, a roadmap entry or
code.

Status (2026-09-21): **contract adopted; seven libraries, three tools and one
optional bundle imported.** Each row below says when its identity arrived from
`usd-vrm-plugins`, with its history. Every other
identity below is *reserved* until the change that creates it lands, and its
row then says so. The shape follows the design
policy's §22 and the workspace discipline `usd-vrm-plugins` and
`usd-mmd-plugins` share: plain libraries apart from plugin bundles, a manifest
beside each component, and two build modes, `ost` and plain CMake.

## 1. Identities

### 1.1 Libraries

| Identity | Directory | Role | Arrives from | Status |
| --- | --- | --- | --- | --- |
| `motionCore` | `libs/motionCore/` | `HumanJoint`, `MotionPose`, `RootMotion`, `MotionChannelSet`, `SourceMetadata`, `MotionClip`, constraints ([MOTION_CONTRACT.md](../design/MOTION_CONTRACT.md)) | `usd-vrm-plugins` `motionCore`, renamed | **imported** 2026-09-19, with history |
| `motionSampling` | `libs/motionSampling/` | sampling with status, interpolation, resample, filter, blend, the pose buffer | `usd-vrm-plugins` `motionRuntime` (its sampling half) | **imported** 2026-09-19, with history |
| `motionRecording` | `libs/motionRecording/` | stream intake, `MotionRecorder`, the `motion-capture-trace` format, replay | `usd-vrm-plugins` `motionRuntime` (its capture half) | **imported** 2026-09-19, with history |
| `motionRetarget` | `libs/motionRetarget/` | `SkeletonDescriptor`, `RetargetMap`, rest correction, root-motion policy, retarget diagnostics ([RETARGETING_POLICY.md](../design/RETARGETING_POLICY.md)) | `usd-vrm-plugins` `vrmRetarget`, its generic half | **imported** 2026-09-19, with history (release v0.2.0) |
| `motionUsd` | `libs/motionUsd/` | `MotionClip` ↔ `UsdSkelAnimation`, a skeleton's joint tokens and rest transforms as values, channels, time codes, metadata ([USD_MAPPING.md](../design/USD_MAPPING.md)) | `usd-vrm-plugins`: `motion_capture`'s clip writer (authoring); `motion_retarget`'s `StageIo` (reading) | **imported** 2026-09-19 (authoring) and 2026-09-20 (reading), with history |
| `motionSource` | `libs/motionSource/` | the format-neutral recorded-source layer: source skeleton and animation, the producer-profile contract, conversion into `MotionClip` | `usd-vrm-plugins` `motionSource` | **imported** 2026-09-19, with history (release v0.4.0) |
| `motionBvh` | `libs/motionBvh/` | BVH syntax and extraction only (design policy §27) | `usd-vrm-plugins` `motionBvh` | **imported** 2026-09-19, with history (release v0.4.0) |

### 1.2 Bundles, tools and data

| Identity | Kind | Directory | Role | Arrives from | Status |
| --- | --- | --- | --- | --- | --- |
| `execMotion` | optional plugin bundle (`usd-exec`) | `plugins/execMotion/` | vendor-neutral OpenExec nodes, each a thin wrapper over a library call (design policy §21) | `usd-vrm-plugins` `execMotion` | **imported** 2026-09-20, with history (release v0.5.0); the only member that needs OpenExec, and `USDMOTION_BUILD_EXEC_MOTION=OFF` builds the workspace without it |
| `motion_inspect` | CLI | `tools/motionInspect/` | reports on a motion stage or clip | new | reserved |
| `motion_convert` | CLI | `tools/motionConvert/` | a recorded source + a named profile → a motion stage | `usd-vrm-plugins` `motion_bvh_convert` | **imported** 2026-09-19, with history (release v0.4.0); it authors through `motionUsd` |
| `motion_bvh_inspect` | CLI | `tools/motionBvhInspect/` | what a BVH file holds, and which profiles fit it | `usd-vrm-plugins` `motion_bvh_inspect` | **imported** 2026-09-19, with history (release v0.4.0) |
| `motion_record` | CLI | `tools/motionRecord/` | a recorded trace → a motion stage | `usd-vrm-plugins` `motion_capture` | **imported** 2026-09-19, with history (release v0.3.0); it authors through `motionUsd` |
| producer profiles | package data | `profiles/motion/` | one declarative file per producer and export preset | `usd-vrm-plugins` `profiles/motion/` | **imported** 2026-09-19, with history (release v0.4.0); installed to `share/usd-motion-plugins/profiles/motion/` |

Libraries are plain static CMake libraries. Names follow the siblings'
workspace discipline (WS-O1, decided 2026-09-19;
[DESIGN_POLICY.md §42.7](../design/DESIGN_POLICY.md#427-identities-are-lower-camel-as-in-the-siblings)):

- **An identity is lower-camel**, and it is also its directory name
  (`libs/motionCore/`), its CMake package name
  (`find_package(motionCore CONFIG)`) and its exported target
  (`motionCore::motionCore`). The in-tree alias is the same, so a consumer's
  `target_link_libraries` line is the same inside and outside this workspace.
- **A CLI's command is snake_case** in a lower-camel directory, as
  `usd-vrm-plugins`' and `usd-mmd-plugins`' tools are. An imported tool
  keeps its command, so `motion_bvh_inspect` is still `motion_bvh_inspect`.
- **The C++ namespace is `openstrata::motion`** (design policy §23). The
  include root is the identity (`#include "motionCore/MotionPose.h"`).
- **`motionCore`, `motionSource` and `motionBvh` keep their
  `usd-vrm-plugins` identity when they move.** That repository never holds a
  copy at the same time as this one (its WORKSPACE.md §9.2, rule 1), and a
  version tells the two packages apart. What changes is the types inside
  them, which are renamed on arrival.

### 1.3 Deliberately not here

| Not here | Where it lives | Why |
| --- | --- | --- |
| VMC, mocopi, VRChat OSC, `osc`, `liveTransport`, tracker assignment and solve | `motion-connectors` | device and protocol connectivity (design policy §3.1) |
| VRMA reading, the VRM humanoid binding, expressions, look-at, `execVrm` | `usd-vrm-plugins` | VRM semantics (design policy §3.2, §26) |
| VMD reading, MMD IK and append evaluation, the MMD role table | `usd-mmd-plugins` | MMD semantics (design policy §3.2, §42.4) |
| physical simulation | `usd-physics-plugins` | simulation (design policy §3.3) |
| scheduling, application execution and OpenExec driving policy | `usd-stage-runner` | execution (design policy §3.3) |
| runtime composition | `usd-avatar-runtime` | composition (design policy §3.3) |
| a generator's model, training or inference | never in this ecosystem's core | design policy §3, §24 |

## 2. Dependency directions

### 2.1 Inside the repository

```text
motionCore ──────→ OpenUSD foundation types only (gf, tf, vt)
motionSampling ──→ motionCore
motionRecording ─→ motionCore, motionSampling
motionRetarget ──→ motionCore
motionUsd ───────→ motionCore, motionSampling, OpenUSD (usd, sdf, usdSkel)
motionSource ────→ motionCore
motionBvh ───────→ motionSource
execMotion ──────→ motionCore, motionSampling, motionRecording, motionUsd,
                   OpenExec
tools/* ─────────→ the libraries they name (motion_convert: motionBvh,
                   motionSource, motionUsd; motion_record: motionRecording,
                   motionSampling, motionUsd), OpenUSD stage authoring
```

This is the design policy's §24 with the recorded-source pair added.
`execMotion` reaches `motionUsd` for one call, `PoseFromStageSample`
([USD_MAPPING.md §7](../design/USD_MAPPING.md#7-reading-usd-back)): the rule
that turns a `UsdSkelAnimation`'s already-resolved arrays into a pose. It does
not make the bundle a stage reader — the values arrive through exec inputs and
the call takes values — but it is the one edge here into a library that holds
stage API, and it carries OpenUSD's `usdGeom` in transitively, which nothing
in the bundle uses.
`motionRetarget` depends on `motionCore` alone (WS-O2, decided 2026-09-19).
`usd-vrm-plugins`' `vrmRetarget` also depended on its runtime library, for one
resample option; the option was removed on arrival, and a caller that wants a
uniform timeline resamples before it retargets.

### 2.2 Forbidden

| Edge | Why |
| --- | --- |
| any component → `usd-vrm-plugins`, `usd-mmd-plugins`, `motion-connectors`, `usd-avatar-runtime` | the ecosystem's direction is fixed (design policy §19.3, §39) |
| `motionCore` → OpenUSD stage, Sdf, plug or file-format APIs | a value contract, usable with no stage |
| any library → OpenExec | OpenExec is an optional layer above (design policy §21) |
| any library → a network, device, ML, UI or rendering dependency | design policy §24 |
| `motionCore`, `motionSampling` → `motionSource`, `motionBvh` | nothing in the core knows a file format exists |
| `motionSource` → a reader; a reader → another reader | readers hang off the format-neutral layer, never the reverse |
| `motionBvh` → a producer name in code, or a default profile | producer semantics are declarative data |
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
| `usd-mmd-plugins` | `motionCore` (and `motionRetarget` for map validation) through `mmdMotionAdapter` | its WORKSPACE.md §2.4, Phase 9 |
| `motion-connectors` | `motionCore`, `motionRecording` | not yet written |
| `usd-avatar-runtime` | any, and `execMotion` | not yet written |

### 2.4 Enforcement

Gates, added with the code they guard, as in the sibling repositories: every
edge declared in the component's manifest and validated by
`ost plugin test --workspace --graph-only`; a link-line check per library
(`motionCore` links no OpenUSD beyond its foundation types); and an include
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

WS-O1, the names, was decided on 2026-09-19 (§1.2).

| Id | Question | Resolve by |
| --- | --- | --- |
| ~~WS-O2~~ | **Decided 2026-09-19: `motionCore` alone**, as design policy §24 draws it (§2.1). Was: whether `motionRetarget` depends on `motionSampling`, as `vrmRetarget` depended on its runtime library for one resample option | the import of `vrmRetarget`'s generic half |
| WS-O3 | Design policy §26 sketches `plugins/motion-bvh/`; the imported BVH reader is a plain library and registers nothing. A BVH `SdfFileFormat` would be a separate, thin bundle, created only if opening `.bvh` directly is wanted | a consumer that wants it |
