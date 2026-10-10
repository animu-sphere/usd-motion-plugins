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
| `motionCore` | `libs/motionCore/` | `HumanJoint`, `MotionPose`, `RootMotion`, `MotionChannelSet`, `SourceMetadata`, `MotionClip`, constraints and signed-permutation basis arithmetic ([MOTION_CONTRACT.md](../design/MOTION_CONTRACT.md)) | `usd-vrm-plugins` `motionCore`, renamed | **imported** 2026-09-19, with history |
| `motionSampling` | `libs/motionSampling/` | sampling with status, interpolation, resample, filter, blend, the pose buffer | `usd-vrm-plugins` `motionRuntime` (its sampling half) | **imported** 2026-09-19, with history |
| `motionRecording` | `libs/motionRecording/` | stream intake, `MotionRecorder`, the `motion-capture-trace` format, replay | `usd-vrm-plugins` `motionRuntime` (its capture half) | **imported** 2026-09-19, with history |
| `motionRetarget` | `libs/motionRetarget/` | `SkeletonDescriptor`, `RetargetMap`, rest correction, root-motion policy, retarget diagnostics ([RETARGETING_POLICY.md](../design/RETARGETING_POLICY.md)) | `usd-vrm-plugins` `vrmRetarget`, its generic half | **imported** 2026-09-19, with history (release v0.2.0) |
| `motionUsd` | `libs/motionUsd/` | `MotionClip` ↔ `UsdSkelAnimation`, a skeleton's joint tokens and rest transforms as values, channels, time codes, metadata; evaluated target-local arrays to a composed skeleton's animation/binding override ([USD_MAPPING.md](../design/USD_MAPPING.md)) | `usd-vrm-plugins`: `motion_capture`'s clip writer (authoring); `motion_retarget`'s `StageIo` (reading) | **imported** 2026-09-19 (authoring) and 2026-09-20 (reading), with history; target-local writer added 2026-10-06 |
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
| VMC, mocopi, VRChat OSC, OpenXR, WebXR, MediaPipe, transports, SDK/browser APIs, raw capture, connector sessions/endpoints/diagnostics and actor/source clock normalization | `motion-connectors` | acquisition (design policy §3.1, §44) |
| `MotionFrame`, `IMotionConnector`, `TrackerObservation`, regions and operator assignment | `motion-connectors` | acquisition envelope and observation organization; direct solve retained there pending the motion-owned input contract ([MOTION §11.1](../design/MOTION_CONTRACT.md#111-a-tracker-observation-gets-no-type-here)) |
| connector-to-motion intake bridge, actor routing and restart/alignment policy selection | external composition, preferably `usd-avatar-runtime` | motion libraries never consume connector types (§44.3) |
| VRMA reading, the VRM humanoid binding, expressions, look-at, `execVrm` | `usd-vrm-plugins` | VRM semantics (design policy §3.2, §26) |
| VMD reading, MMD IK and append evaluation, the MMD role table | `usd-mmd-plugins` | MMD semantics (design policy §3.2, §42.4) |
| physical simulation | `usd-physics-plugins` | simulation (design policy §3.3) |
| scheduling, application execution and OpenExec driving policy | `usd-stage-runner` | execution (design policy §3.3) |
| avatar lifecycle, evaluator phases/ordering, discovery, capabilities, transactional state and renderer/Hydra publication | `usd-avatar-runtime` | avatar orchestration (design policy §3.3, §43) |
| a generator's model, training or inference | never in this ecosystem's core | design policy §3, §24 |

## 2. Dependency directions

### 2.1 Inside the repository

Implemented graph, including WS-O4's public typed-reader edge (§2.5).

```text
motionCore ──────→ OpenUSD foundation types only (gf, tf, vt)
motionSampling ──→ motionCore
motionRecording ─→ motionCore, motionSampling
motionRetarget ──→ motionCore
motionUsd ───────→ motionCore, motionRetarget, OpenUSD (usd, sdf, usdGeom, usdSkel)
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
| `motionCore` → filtering, recording or retarget implementations | the lowest canonical value contract, not a processing layer |
| any motion API → `MotionFrame`, `IMotionConnector` or `TrackerObservation` | connector-owned types never enter canonical intake (§2.6) |
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

`workspace_connector_boundaries` adds the §2.6 source/include and manifest
scan and a configured direct/transitive link-property closure for all libraries,
tools and the optional bundle. `cmake/UsdMotionBoundaryGraph.cmake` follows
aliases, private/interface links, imported configurations and dependency
properties, retaining every generator-expression alternative. The companion
`workspace_connector_boundaries_selftest` uses refusal fixtures and real
compiler-free CMake graphs (aliases, cycles, conditional links and imported
SDK locations). The hand-written docs-check workflow runs the source/manifest
and fixture lanes without an OpenUSD runtime; workspace CTest CI runs the
configured SDK closure and the installed canonical consumer. The installed
lane also snapshots every exported motion library's closure from the copied
consumer, using a copied test helper and no workspace motion targets.

Windows OpenUSD 26.08's imported `arch` foundation target already links
`Ws2_32`. Only that inherited edge is allowed, requiring an imported `arch`
target with an `usd_arch` Windows library location. Motion-owned targets,
other wrappers and socket calls/includes receive no exception. This retains
the foundation allowance of §2.5; it does not establish full WS-O5 isolation.

The shared source rules distinguish provenance strings from dependency names,
connector-owned types and direct source-name comparisons. The core/processing
component product scans reuse that distinction; the recorded-source/BVH gates
retain their stricter no-producer assumptions. These are lexical structural
guards over `include/` and `src/`, not C++ parsing, data-flow analysis or a
replacement for the existing component binary audits. New SDK spellings and
indirect source selectors require review and new refusal fixtures.

### 2.5 Runtime boundary target

`motionCore/Validation.h` now owns read-only pose/clip reports and primitive
checks; `motionRetarget/Validation.h` owns rest, skeleton and configuration
reports using those checks and the existing rig diagnostics. They add no
library edge or stage dependency. Their installed public contract and mutable
validation boundary are in
[MOTION §14](../design/MOTION_CONTRACT.md#14-generic-validation-ownership).

`motionUsd` owns stage time-code conversion and validation over the complete
body/channel/gaze input union, including finite seconds and adjacent spans.
The common reader's fallback and strict reader's refusal policies are in
[USD §7.3](../design/USD_MAPPING.md#73-reader-time-code-policy); they reuse the
existing graph without adding a retarget or runtime dependency.

The accepted [motion/runtime ownership policy](../design/DESIGN_POLICY.md#43-motion-and-avatar-runtime-boundary)
requires `motionUsd` to return motion-domain skeleton/rest values as well as
clips. The §2.1 graph and §1 identities describe the implemented components;
the new reader contract is
[USD_MAPPING.md §7.2](../design/USD_MAPPING.md#72-motion-domain-reader-results).

**WS-O4 decided, 2026-10-07: add a public `motionUsd` → `motionRetarget`
dependency for the typed reader extension.** `SkeletonDescriptor`,
`SourceRestPose`, their builders and validators stay in `motionRetarget`.
`motionUsd` reads and validates USD values, then invokes
`BuildSkeletonDescriptor` and, for an explicitly semantic source skeleton,
`BuildSourceRestPose`. It returns owned values; it does not implement a second
decomposition, hierarchy or rest-construction algorithm. A generic skeleton
descriptor does not require humanoid source-rest extraction.

The accepted reader graph, implemented locally and unreleased, is:

```text
motionUsd ───────→ motionCore, motionRetarget,
                  OpenUSD (usd, sdf, usdGeom, usdSkel)
motionRetarget ──→ motionCore
```

The dependency is public because installed reader headers expose
`motionRetarget` values. The existing direct `motionCore` edge remains: the
reader also exposes canonical clip/pose values. `motionRetarget` keeps its
stage-free value/building API and gains no reverse edge. Existing include
roots, namespaces, exported targets and builders remain available to callers
that do not use a USD reader.

Moving these values/builders into `motionCore` or a new lower component is not
selected. Current consumers already use the installed `motionRetarget`
surface, whose rest header also contains mapping/correction APIs. Splitting
that surface would require a separate public-type and package migration
without evidence that the typed reader needs one. This decision reuses the
existing owner and leaves neutral-value migration to WS-O5.

The structural decision preceded implementation. The manifest, public CMake
links, installed `find_dependency` and include/link gates now declare this
edge. `ReadMotionSkeleton` returns owner-built values with explicit generic
or semantic-source interpretation; `ReadCanonicalMotionStage` fills the
additive descriptor/source-rest fields. Owner tests, graph validation,
standalone builds and clean installed consumption cover the boundary.
The installed-consumer configure resolves `motionUsd` before explicitly
finding `motionRetarget`, verifying transitive package discovery.

Runtime adoption and duplicate-removal evidence remain required to close
Runtime Boundary Phases 2-3. The permissive reader's raw-array/fallback
contract is unchanged.

OpenUSD stage/schema APIs stay in `motionUsd`. Full OpenUSD dependency
isolation is the target, but `motionCore` and the processing APIs currently
expose foundation types (`gf`/`tf`/`vt`); this documentation change does not
replace those types. WS-O5 must settle neutral math/value types and the migration
of existing public APIs before ABI stabilization can claim full isolation.

The runtime consumes installed owner APIs through adapters that marshal,
invoke and publish. It retains its state ABI, layout IDs and avatar
orchestration; it does not create canonical motion structs or a joint
vocabulary. Tests of that consumption belong to the runtime; correctness
tests of motion algorithms and USD conversion belong here. The ordered
acceptance gates are in [runtime-boundary.md](../roadmap/runtime-boundary.md).

### 2.6 Connector boundary target

The accepted [connector boundary policy](../design/DESIGN_POLICY.md#44-motion-and-connector-boundary)
fixes the dependency direction as `motion-connectors` → installed motion
packages, never the reverse. `MotionFrame`, `IMotionConnector` and
`TrackerObservation` remain connector-owned and are never included, linked or
accepted by motion APIs. The existing intake accepts `MotionPose`; any extended
sample/input metadata belongs to this repository
([MOTION §9.1](../design/MOTION_CONTRACT.md#91-canonical-intake-and-acquisition-envelopes)).
No new component identity or library edge is adopted by this documentation
change.

The connector-to-intake bridge belongs to external composition, preferably
`usd-avatar-runtime`. It consumes both installed packages, routes actors and
maps observations to canonical values. Source observation and protocol-clock
interpretation stay upstream; runtime policy chooses alignment/reset, and
motion APIs implement generic buffering and processing effects. Neither
source-specific live wrappers nor connector session management move here.

Dependency gates must inspect component manifests/library graphs, public and
private includes, and direct/transitive link dependencies for connector
packages, OpenXR, MediaPipe, OSC, WebSocket implementations, device SDKs,
browser APIs and network sockets. Provenance strings and documentation naming
a source are not dependencies; source-name branches in generic processors
are boundary defects. A core gate must also keep filtering, recording,
retarget implementations, stage APIs and OpenExec above `motionCore`.

The existing `motionRecording` intake and `motionSampling` buffer enforce
finite motion timelines, with explicit reset/alignment operations and no new
dependency edge ([MOTION §9.1.1](../design/MOTION_CONTRACT.md#911-existing-temporal-primitives)).
`motionRecording` also owns explicit `MotionInputState` availability, retaining
the pose intake and metadata; its contract is
[MOTION §9.1](../design/MOTION_CONTRACT.md#91-canonical-intake-and-acquisition-envelopes).

The current graph remains §2.1, including its existing OpenUSD foundation
types. MC-O8 defines the generic solve's
motion-owned input and component contract following the ownership review in
[MOTION §11.1](../design/MOTION_CONTRACT.md#111-a-tracker-observation-gets-no-type-here).
Any future solve must consume motion-owned values and first update component
placement, graph declarations and tests here. `TrackerObservation` is not
moved as part of that evaluation. The ordered API, bridge-consumption, solve
evaluation and enforcement gates are in
[connector-boundary.md](../roadmap/connector-boundary.md).

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
- Each component is a self-contained CMake project that builds in the
  workspace, standalone, and against installed packages. What they share is
  `cmake/`, reached from every component by a relative path:
  `UsdMotionProject.cmake` (the version and the project policy),
  `UsdMotionOpenUsd.cmake` (the pin, OpenUSD's targets as
  `usdmotion::pxr::<name>`, and the OpenExec probe), `UsdMotionTargets.cmake`
  (a sibling package, and `/utf-8`), `UsdMotionInstall.cmake`,
  `UsdMotionStage.cmake`, `UsdMotionTesting.cmake` and
  `UsdMotionUtf8CodePage.cmake`. A component's `CMakeLists.txt` keeps what is
  its own: its sources, its `PUBLIC` and `PRIVATE` links, its definitions, its
  install and its tests.
- A build writes nothing into the source tree. Each member is staged in its
  binary directory, laid out as the member is (a tool's `bin/`, a bundle's
  `lib/` beside its `plugin/resources/`), one stage per configuration under a
  multi-config generator; the build's tests run against that stage. What ships
  is what the install rules install: `ost plugin build` installs the bundle
  into its target-local stage, which `ost plugin test` and `ost plugin package`
  read, and `ost build` takes each tool from its member's `bin/` in the root
  build tree.

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
| ~~WS-O4~~ | **Decided 2026-10-07: public `motionUsd` → `motionRetarget` edge**, reusing the existing types/builders without moving them (§2.5); edge and typed readers implemented locally, unreleased | runtime adoption remains Runtime Boundary Phases 2-3 |
| WS-O5 | Neutral math/value types and migration of the public foundation-type APIs to achieve full OpenUSD isolation (§2.5) | Runtime Boundary Phase 5, before ABI freeze |
