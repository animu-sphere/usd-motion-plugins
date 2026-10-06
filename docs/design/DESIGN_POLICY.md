# usd-motion-plugins - Design and Implementation Policy

> Status: **accepted** as the repository's scope and placement policy,
> 2026-09-17. Implementation status is owned by
> [reference/CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md); this
> document defines boundaries and decisions, not the implementation inventory.
> Repository: `animu-sphere/usd-motion-plugins`
> Scope: vendor-neutral motion representation, retargeting, recording, and
> OpenUSD interoperability
> Primary consumers: `usd-vrm-plugins`, `usd-mmd-plugins`,
> `usd-avatar-runtime`, `motion-connectors`
>
> Four focused documents own the detailed contracts, and each wins on its own
> area: [MOTION_CONTRACT.md](MOTION_CONTRACT.md) owns in-memory motion values
> and recording; [RETARGETING_POLICY.md](RETARGETING_POLICY.md) owns generic
> retargeting; [USD_MAPPING.md](USD_MAPPING.md) owns the motion stage; and
> [EXEC_CONTRACT.md](EXEC_CONTRACT.md) owns OpenExec evaluation. Component
> identities and dependency edges are
> [architecture/WORKSPACE.md](../architecture/WORKSPACE.md)'s.
>
> Section numbers are stable because sibling repositories cite this policy. A
> revision adds subsections or appends sections and does not change what an
> existing number means. Section 42 records decisions taken since adoption.

---

## 1. Project overview

`usd-motion-plugins` is the canonical owner of format-independent motion
semantics, mathematics and interchange for OpenUSD-oriented workflows. It
represents, canonicalizes, transforms, retargets, records, evaluates, and
bridges reusable motion data.

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

Acquisition, avatar-format interpretation, simulation, and application
execution remain outside this repository.

The central rule is:

> **Motion is represented independently of its transport, source product,
> avatar format, and runtime host.**

The repository must not become a second VRM repository, a second MMD
repository, a device adapter collection, or a complete avatar runtime.

## 2. Goals

The repository provides a reusable path from external or canonical motion to
motion data that can be consumed by different avatar and runtime repositories.
The design optimizes for:

- stable, format-neutral contracts;
- standard OpenUSD representation;
- deterministic offline conversion;
- low-overhead sampling and evaluation;
- explicit coordinate-space and time semantics;
- generic retargeting independent of VRM and MMD;
- tests that do not require a renderer or application runtime.

The exact value, stage, and evaluation rules live in the focused contracts.

## 3. Non-goals

The following responsibilities are outside the core motion layer.

### 3.1 Device and protocol connectivity

WebXR, MediaPipe, VMC/OSC, mocap devices, camera tracking, trackers, vendor
SDKs, network transports, discovery, and reconnect logic belong to
`motion-connectors` or an integration package. This repository consumes an
already decoded `MotionPose` or `MotionStream`.

Actor/source clock normalization and sensor protocol handling belong to
`motion-connectors`; motion libraries operate on explicit timestamps and
caller-supplied timing policies.

This includes VMC, mocopi, VRChat OSC, OpenXR, WebXR and MediaPipe acquisition,
UDP/WebSocket/OSC transport, device SDKs, browser APIs, raw packet capture,
connection/session management, endpoints and protocol diagnostics. The
canonical intake and composition boundary is fixed in §44.

### 3.2 Avatar-format semantics

VRM humanoid metadata, expressions, look-at, spring bones, VRMA semantics, PMX
bone flags, MMD morphs, MMD IK, append evaluation, and MMD physics meaning
belong to their format repositories. This repository supplies generic values,
descriptors, maps, and processing primitives for them to consume.

### 3.3 Runtime orchestration

Application update loops, scheduling, scene lifecycle, network sessions,
OpenExec driving policy, and application state belong to `usd-stage-runner` or
another runtime layer. Runtime composition belongs to `usd-avatar-runtime`.

`usd-avatar-runtime` owns avatar instance lifecycle, evaluator phases and
ordering, provider discovery, capability negotiation, per-avatar transactions,
`EvaluatedAvatarState`, and renderer/Hydra publication. Its motion adapter
marshals inputs, invokes owner APIs and publishes results (§43).

Physical response, including spring bones, rigid-body dynamics, collision
response, cloth-like behavior, and physical constraints, belongs to
`usd-physics-plugins`. The motion layer may provide the kinematic input pose.

### 3.4 Rendering

Hydra delegates, shading, materials, and viewport behavior are outside scope.

## 4. Design principles

### 4.1 Vendor-neutral core

Provider and protocol names may be preserved as provenance or diagnostic data,
but they must not control core behavior. A generally useful capability is
promoted into a generic contract rather than added as a provider branch.

### 4.2 Avatar-format-neutral core

The core operates on semantic joints, skeleton topology, rest poses,
coordinate spaces, and explicit policies. Format repositories construct generic
descriptors and maps from their own bindings or role tables.

### 4.3 Existing OpenUSD schemas first

Use `UsdSkelSkeleton`, `UsdSkelAnimation`, `UsdSkelBindingAPI`, standard USD
attributes, time samples, and composition before introducing a project schema.
A custom schema is justified only when existing schemas, namespaced metadata,
and ordinary composition cannot express a stable concept.

### 4.4 Representation and evaluation are separate

A motion asset, a target skeleton, a binding, a retarget policy, and an
evaluated result are separate concepts. Applying motion to an avatar is a
consumer or runtime concern, not ownership of the source motion asset.

### 4.5 Offline and realtime share the same semantic model

`MotionPose`, `MotionStream`, and `MotionClip` differ in lifetime and storage,
not in the meaning of their values. Recording and sampling reuse the same
contracts in offline and realtime paths.

## 5. Core semantic model

The detailed in-memory model is owned by
[MOTION_CONTRACT.md](MOTION_CONTRACT.md). This policy only fixes its placement:
generic motion values belong here, while acquisition and avatar interpretation
remain outside.

## 5.1 MotionPose

`MotionPose` is the canonical instantaneous motion value. Its fields,
optionality, timestamps, validity rules, metadata, and equality are defined by
[MOTION_CONTRACT.md section 5](MOTION_CONTRACT.md#5-motionpose).

### 5.1.1 Why root motion is separate

World/root motion and hips-local motion have different composition and
retargeting behavior. Their representation and invariants are defined by
[MOTION_CONTRACT.md section 5.3](MOTION_CONTRACT.md#53-root-motion-and-the-hips).

## 5.2 Human joint semantics

The canonical joint vocabulary is versioned and format-neutral. It is not a
VRM bone list, a USD prim-path list, or a target rig binding. Its contents and
hierarchy are owned by
[MOTION_CONTRACT.md section 2](MOTION_CONTRACT.md#2-the-joint-vocabulary).

## 5.3 MotionChannelSet

Channels carry time-varying values that are not body joints. Namespaced
format-specific values may pass through without being interpreted here. The
channel value and naming contract is owned by
[MOTION_CONTRACT.md section 6](MOTION_CONTRACT.md#6-channels).

## 5.4 SourceMetadata

Provenance survives normalization but is never a behavior branch. Its fields,
scope, and relationship to recorded-source provenance are owned by
[MOTION_CONTRACT.md section 7](MOTION_CONTRACT.md#7-sourcemetadata).

## 6. MotionStream

`MotionStream` is a motion data model and processing boundary, not a transport
API. Intake rules and the persistent trace format are owned by
[MOTION_CONTRACT.md sections 9-10](MOTION_CONTRACT.md#9-motionstream-intake).
Connectors own protocol decode and delivery lifecycle.

## 7. MotionClip

`MotionClip` is the finite stored motion sequence used by samplers, recorders,
retargeters, and USD bridges. Its timestamps and metadata are defined by
[MOTION_CONTRACT.md section 8](MOTION_CONTRACT.md#8-motionclip-and-sampling).

## 8. Sampling and interpolation

Sampling, interpolation, resampling, filtering, and blending are generic
processing operations. Their behavior is owned by
[MOTION_CONTRACT.md section 8](MOTION_CONTRACT.md#8-motionclip-and-sampling)
and the `motionSampling` implementation; this policy does not define another
sampler API.

## 9. Coordinate system contract

Inputs are normalized at the reader or connector boundary before entering
shared processing. The canonical basis, time unit, rotation representation,
and conversion rules are owned by
[MOTION_CONTRACT.md section 3](MOTION_CONTRACT.md#3-coordinates-and-units).

## 10. SkeletonDescriptor

`SkeletonDescriptor` is a plain-value description of a target or source
skeleton. Its joints, parents, rest data, and construction boundary are owned
by [RETARGETING_POLICY.md section 2](RETARGETING_POLICY.md#2-skeletondescriptor).
Reading USD values into that description is `motionUsd`'s responsibility.

## 11. RetargetMap

`RetargetMap` expresses explicit semantic correspondence. Format repositories
may build it from their own bindings or role tables; the generic retargeter
does not interpret VRM, MMD, or product-specific names. Its rules are owned by
[RETARGETING_POLICY.md section 3](RETARGETING_POLICY.md#3-retargetmap).

## 12. Retargeting pipeline

Retargeting transforms canonical motion values into a target skeleton without
opening a stage, decoding a file, or owning an avatar format. The inputs,
determinism, diagnostics, and rest-pose correction are owned by
[RETARGETING_POLICY.md](RETARGETING_POLICY.md).

### 12.1 Initial scope

The initial generic surface is the implemented `motionRetarget` contract. Its
current capabilities and tests are recorded only in
[CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md).

### 12.2 Later scope

IK-assisted hands and feet, foot locking, contacts, twist distribution, joint
limits, and locomotion warping are later work only when their APIs remain
format-neutral. The owning future scope is
[RETARGETING_POLICY.md section 8](RETARGETING_POLICY.md#8-later-scope).

## 13. Root-motion policy

Root-motion placement is a retargeting decision, not an implicit side effect.
The published modes and their behavior are owned by
[RETARGETING_POLICY.md section 6](RETARGETING_POLICY.md#6-root-motion).

## 14. Recording

Recording converts a motion stream into a persistent generic clip or trace. It
does not write a VRM, VMD, BVH, or other avatar-format asset. Intake, recording,
and trace rules are owned by
[MOTION_CONTRACT.md sections 9-10](MOTION_CONTRACT.md#10-recording-and-the-trace-format).

## 15. Filtering and stabilization

Filtering is an explicit processing operation. It may be composed by a caller,
but it does not define transport ownership or an application update loop. The
implemented filter and its status behavior are owned by the sampling contract
and the capability matrix.

## 16. OpenUSD bridge

`motionUsd` converts canonical motion values to and from standard `UsdSkel`
structures. It is a library, not a file-format plugin and not a target-avatar
baker. The stage shape, metadata, authoring, and reading rules are owned by
[USD_MAPPING.md](USD_MAPPING.md).

## 17. USD authoring policy

### 17.1 Root prim

The standalone motion asset uses the mapping's `/Animation` root. The exact
prim layout is [USD_MAPPING.md section 2](USD_MAPPING.md#2-the-standalone-motion-stage).

### 17.2 Stage metrics

Stage metrics and sample-time encoding are defined by
[USD_MAPPING.md section 4.1](USD_MAPPING.md#41-time), not by a file reader's
local assumptions.

### 17.3 Unicode

Source names are preserved. Any USD-safe identifier is a deterministic derived
identifier, never a replacement for the source name. The authoring rule is
[USD_MAPPING.md section 3](USD_MAPPING.md#3-the-skeleton).

### 17.4 Metadata

Motion and source provenance are authored under the mapping's namespaces;
runtime-only state is not stored in an asset. See
[USD_MAPPING.md section 5](USD_MAPPING.md#5-metadata).

## 18. USD composition model

Source motion, an avatar asset, and their binding are separate composition
concerns. A baked target-specific animation is a derivative, not a mutation of
the source motion asset. The binding boundary is owned by
[USD_MAPPING.md section 6](USD_MAPPING.md#6-motion-on-an-avatar).

## 19. Avatar repository integration

Avatar repositories consume this layer and retain their own format semantics.
The workspace dependency direction is the binding structural rule in
[WORKSPACE.md section 2](../architecture/WORKSPACE.md#2-dependency-directions).

## 19.1 usd-vrm-plugins

`usd-vrm-plugins` owns VRM and VRMA semantics, humanoid bindings, expressions,
look-at, spring-bone behavior, and VRM-specific metadata. It may construct
generic descriptors and consume the motion libraries.

### 19.2 usd-mmd-plugins

`usd-mmd-plugins` owns PMX/PMD/VMD semantics, MMD bone and morph rules, IK,
append evaluation, and MMD-specific physics meaning. Generic body motion may
be handed to this repository as canonical values; MMD control semantics stay
there.

### 19.3 Avoid circular dependencies

The dependency direction is:

```text
motion-connectors -----> usd-motion-plugins <----- usd-vrm-plugins
                                  ^
                                  |
                           usd-mmd-plugins
```

This repository never depends on an outer avatar or connector repository.

## 20. motion-connectors integration

`motion-connectors` owns the external-world boundary and reports what it
observed in its acquisition envelope. A consumer adapter routes canonical
motion values into this repository's intake; the envelope remains
connector-owned. The motion libraries do not own device discovery, transport,
reconnect logic, or a wall clock. See §44 and
[MOTION_CONTRACT.md §9.1](MOTION_CONTRACT.md#91-canonical-intake-and-acquisition-envelopes).

## 21. OpenExec boundary

OpenExec is an optional evaluation adapter above ordinary C++ motion libraries.
`execMotion` wraps library calls and does not become a second implementation of
their semantics. The driver, input, and scene-side rules are owned by
[EXEC_CONTRACT.md](EXEC_CONTRACT.md).

## 22. Repository structure

The actual component identities, directories, package names, and reserved
entries are maintained only in
[WORKSPACE.md section 1](../architecture/WORKSPACE.md#1-identities). This
policy does not maintain a second proposed tree.

## 23. C++ namespace policy

The shared public namespace is `openstrata::motion`. Format-specific code
belongs outside the core namespace. Component identities, include roots, and
exported targets are maintained in
[WORKSPACE.md section 1.2](../architecture/WORKSPACE.md#12-bundles-tools-and-data).

## 24. Dependency policy

Keep the core dependency graph narrow. The current internal edges and
forbidden edges are owned by
[WORKSPACE.md section 2](../architecture/WORKSPACE.md#2-dependency-directions);
external pins are owned by [DEPENDENCIES.md](../architecture/DEPENDENCIES.md).
Shared libraries do not depend on networks, devices, ML, UI, rendering,
physics, or OpenExec.

## 25. Python API

Python bindings are a possible inspection and conversion surface, not a second
semantic implementation. Their status is tracked in the capability matrix and
future work in the roadmap; no Python API is defined here.

## 26. File-format ownership

A format belongs here when it is primarily a generic motion container and can
be converted into the canonical motion model without importing avatar
semantics.

```text
BVH  -> usd-motion-plugins
NPZ  -> only after a versioned generic payload contract
VRMA -> usd-vrm-plugins
VMD  -> generic decoding may be shared; MMD interpretation stays in usd-mmd-plugins
```

## 27. BVH direction

BVH parsing and extraction are generic format responsibilities. Producer units,
axes, rest interpretation, and profile matching belong to the declarative
profile/conversion layer. The current component boundary is recorded in
[WORKSPACE.md section 1.1](../architecture/WORKSPACE.md#11-libraries) and the
capability matrix.

## 28. Generic NPZ direction

Do not treat arbitrary NPZ files as a contract. Define a versioned payload
schema first, then add a reader or writer at the format boundary.

## 29. Diagnostics

Diagnostics are stable values with owned codes and clear layer boundaries.
Current codes and their raisers are catalogued only in
[reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md).

## 30. Testing policy

Tests are layered with the code they protect. A capability is documented as
implemented only when its test evidence is recorded in
[CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md). Cross-repository
tests belong to the consumer or integration repository when adding them here
would create a dependency cycle.

### 30.1 motionCore

Test the value contract, validity, timestamps, provenance, and root/body
separation in `motionCore`.

### 30.2 sampling

Test sampling status, interpolation, filtering, resampling, and blending in
`motionSampling`.

### 30.3 retargeting

Test explicit maps, rest correction, missing joints, root policy, and
diagnostics in `motionRetarget`.

### 30.4 recording

Test intake, trace round trips, recording, and replay in `motionRecording`.

### 30.5 USD bridge

Test stage shape, resolved `UsdSkel` values, metadata, and round trips in
`motionUsd`.

### 30.6 Cross-repository integration

Keep avatar-format, connector, physics, and runtime integration tests in the
repository that owns that boundary unless a package-level test is sufficient.

## 31. Performance policy

Descriptors, maps, clips, and sampling state should be reusable across frames.
Do not put repeated stage traversal or application scheduling in a per-sample
core operation.

## 32. Threading policy

Semantic libraries do not own a global worker pool or runtime scheduler.
Mutable stream and filter state documents its thread-safety; immutable clips
and descriptors remain shareable.

## 33. Serialization stability

Raw C++ layout is not a persistent ABI. USD, trace, and future payload formats
carry explicit versions, and migrations happen at format boundaries.

## 34. Security and robustness

Readers and bridges validate finite values, timestamps, quaternion values,
sample counts, hierarchy depth, parent indices, duplicate mappings, and input
sizes. Queues and external buffers are bounded by their owning connector or
runtime.

## 35. Release strategy

Release scope is recorded in the roadmap while a release is planned, and in
its immutable record under [releases/](../releases/) after publication. This
section defines no second release inventory. Unfinished work belongs in
[roadmap/current.md](../roadmap/current.md).

## 36. Initial vertical slice

The initial vertical slice is historical design context. The current
implementation is reported by the capability matrix, and remaining work is
reported by the roadmap; this section is not a second plan.

## 37. Migration from usd-vrm-plugins

Generic code moves here as installable, tested packages with its history.
Consumers remove duplicate copies only after package consumption and parity
evidence are complete. Current migration work is tracked in
[roadmap/current.md](../roadmap/current.md); this section does not repeat its
checklist.

## 38. Decision rules for repository placement

Put a feature in `usd-motion-plugins` when it represents, canonicalizes,
transforms, retargets, records, evaluates, or bridges motion independently of
an avatar format or transport.

Put it in `motion-connectors` when it acquires motion or owns a device,
protocol, network, source-native coordinate interpretation, or reconnect
lifecycle. A device/protocol name required to explain the behavior is an
acquisition concern; generic processing of `MotionPose` / `MotionClip` is a
motion concern. Filtering, interpolation, semantic recording and target
skeleton adaptation belong here.

Put it in `usd-vrm-plugins` or `usd-mmd-plugins` when it requires the
corresponding avatar format's semantics, bindings, control rig, or metadata.

Put it in `usd-physics-plugins` when it simulates physical response.

Put it in `usd-stage-runner` when it schedules, executes, or owns scene updates,
OpenExec driving, or application state.

Put it in `usd-avatar-runtime` when it composes motion, avatar semantics,
physics, and application state, including the bridge from a connector's
acquisition envelope to motion-owned intake (§44.3).

## 39. Architectural invariants

1. This repository never depends on `usd-vrm-plugins`, `usd-mmd-plugins`,
   `motion-connectors`, a runtime, or a physics implementation.
2. Device, provider, and avatar-format names never control generic behavior.
3. Root motion and hips-local motion remain distinct.
4. Canonical time is in seconds and missing joints are representable state.
5. Retargeting consumes explicit generic descriptors and maps.
6. OpenUSD authoring prefers standard `UsdSkel` structures.
7. OpenExec remains optional and layered above ordinary libraries.
8. Recording produces generic motion before any avatar-format export.
9. Source names survive derived USD-safe identifiers.
10. Offline clips and realtime streams share pose semantics.
11. Filtering, retargeting and semantic recording have one canonical
    implementation here; consumers invoke those APIs instead of duplicating them.
12. Device, protocol and network dependencies never enter this repository.
13. `motionCore` is usable without a `UsdStage`, renderer, network or device;
    source-specific coordinate interpretation stays outside generic processing.

## 40. Definition of success

The repository succeeds when new connectors and avatar formats can reuse the
same motion values and processing libraries without adding provider or format
logic to the core, and when runtime systems can consume those libraries
without the libraries owning runtime orchestration.

## 41. Policy summary

```text
motion-connectors    = acquire
usd-motion-plugins   = represent / transform / retarget / record / bridge
avatar plugins       = interpret
usd-physics-plugins  = simulate
usd-stage-runner     = execute
usd-avatar-runtime   = compose
```

The current tree, capabilities, diagnostics, and incomplete work are each
owned by their category documents; this summary is not an implementation list.

## 42. Decisions recorded since adoption

Sections 42.1-42.6 were taken on 2026-09-17 while the sibling repositories
aligned their documentation. Sections 42.7 and 42.8 were taken with the
workspace and first imported diagnostic. Each decision is recorded here and
applied by the owning document.

### 42.1 The core is imported from `usd-vrm-plugins`, not rewritten

Existing generic implementations move here with their history and tests,
instead of being rewritten. Device and protocol input goes to
`motion-connectors`. The receiving-side move rules are in
[WORKSPACE.md section 3](../architecture/WORKSPACE.md#3-moving-code-in).

### 42.2 Names are this policy's, applied on arrival

Imported public types are renamed to this repository's generic vocabulary on
arrival. The resulting identities and package names are maintained in
[WORKSPACE.md section 1](../architecture/WORKSPACE.md#1-identities).

### 42.3 Migration phases are always qualified

This policy's phases are written as **Migration Phase A-F** so they are not
confused with unrelated phases in sibling repositories.

### 42.4 A format repository evaluates its own control semantics first

A format repository evaluates its control rig or format-specific semantics and
hands this repository generic body motion. A `MotionClip` is not a hidden home
for avatar control semantics.

### 42.5 The canonical basis includes a forward axis

The canonical basis includes +Z forward. The exact conversion and evidence are
owned by [MOTION_CONTRACT.md section 3](MOTION_CONTRACT.md#3-coordinates-and-units).

### 42.6 Evidence travels with the code

Evidence that justifies a moved contract stays with the contract and its tests.
The capability matrix records what has actually landed here.

### 42.7 Identities are lower-camel, as in the siblings

Library identities are lower-camel and are shared by their directory, CMake
package, and exported target. CLI commands are snake_case. The binding names
are in [WORKSPACE.md section 1.2](../architecture/WORKSPACE.md#12-bundles-tools-and-data).

### 42.8 Diagnostic codes are named, and renamed on arrival

Codes use the `MOTION_<AREA>_<EVENT>` form. An imported code takes this
repository's prefix while retaining its event name; consumer-owned codes pass
through unchanged. The catalog is
[reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md).

## 43. Motion and avatar runtime boundary

**Accepted ownership policy, 2026-10-06.** The API extensions described here
are intended contracts; their implementation status remains in the capability
matrix. The ordered migration and completion gates are in
[runtime-boundary.md](../roadmap/runtime-boundary.md). This extends the original
Migration Phase A-F with a separate **Runtime Boundary Phase 1-5** track.

### 43.1 Canonical ownership

| Area | Owner and boundary |
| --- | --- |
| Motion values | This repository: `MotionPose`, `MotionClip`, stream semantics, scalar/named channels, generic gaze/look-at observations, root motion, confidence, timestamps, source rest, skeleton descriptors and canonical humanoid vocabulary. |
| Motion mathematics | This repository: sampling, interpolation, extrapolation/hold, filtering, blending, resampling, retargeting, rig diagnostics, root conversion, source/target rest handling, validation, recording and replay primitives. |
| USD motion interchange | `motionUsd`: clip/pose/sample read and write, UsdSkel animation, stage metadata, source skeleton/rest extraction, USD-to-motion skeleton conversion and generic units/axis/time-code/placement interpretation. |
| Avatar orchestration | `usd-avatar-runtime`: when, for which avatar and in which order to invoke algorithms, adapt values to runtime state, and publish the evaluated result. |
| Avatar-format meaning | `usd-vrm-plugins` / `usd-mmd-plugins`: VRM expressions, LookAt and humanoid rules; MMD morphs, IK/control rules and PMX/VMD interpretation. |
| External intake | `motion-connectors`: WebXR/OpenXR, MediaPipe, controllers, devices, network streams, actor/source clock normalization and sensor protocols. |

A generic look-at observation does not interpret VRM LookAt. Generic validation
of a caller-supplied required-bone set does not define a format's required set.
Opaque namespaced channels remain generic values; runtime-specific channel
names must not become fixed semantics in `motionCore`.

### 43.2 Absorbing motion-domain work from the runtime

`motionUsd` is the owner of the generic work currently exposed through the
runtime's `adapters/motion-usd/StageClip`: stage time-code validation, stage
reading, source skeleton extraction, source-rest construction, a coherent
clip-plus-rest result, and generic motion placement checks. The runtime should
consume that result rather than rebuild a motion-domain convenience object.
The planned reader contract is
[USD_MAPPING.md §7.2](USD_MAPPING.md#72-motion-domain-reader-results).

The generic part of `adapters/usd/SkeletonBinding` also belongs here: reading
UsdSkel joints and rests, decomposition, topology checks, building a
`SkeletonDescriptor`, source/target rest construction, generic mapping helpers
and skeleton validation. Pure value algorithms remain in their motion library;
`motionUsd` invokes them at the USD boundary. Avatar IDs, layout versions,
format-provided bindings, per-avatar state and publication remain runtime
adapter concerns.

Generic validation must have owner APIs, rather than copies in runtime
adapters. The value-validation contract is
[MOTION_CONTRACT.md §14](MOTION_CONTRACT.md#14-generic-validation-ownership);
skeleton/rest/retarget validation is owned by `motionRetarget`. Runtime checks
of its own state/input ABI remain runtime-owned.

### 43.3 Adapter-friendly C++ APIs and ABI mapping

Canonical algorithm APIs are C++17 or later. Prefer value types, immutable
views and reusable configuration. Sampling returns pose plus status;
retargeting reuses its descriptor/map/rest configuration and exposes owner
diagnostics; recording returns a generic clip. Preserve existing public APIs
where they already serve these needs instead of adding equivalent wrappers.

Algorithm APIs must not require avatar lifecycle, scheduling, runtime state,
renderer handles, or format-specific types. OpenUSD stage/schema dependencies
belong in `motionUsd`. Full OpenUSD dependency isolation is the target;
existing `gf`/`tf`/`vt` foundation types remain a current API constraint recorded
in [WORKSPACE.md §2.5](../architecture/WORKSPACE.md#25-runtime-boundary-target).
Resolve that constraint explicitly before the ABI freeze.

When a runtime ABI needs a different shape, use only the necessary mapping:

```text
motion-domain value -> adapter mapping -> avatar-runtime input/state contract
```

The mapping may marshal or reorder values for the runtime's layout. It must
not create a second canonical joint vocabulary or motion representation, copy
one package's struct layout as the other's canonical contract, or make
`EvaluatedAvatarState` the canonical motion value. C++ object layout is not a
stable cross-package ABI (§33); freeze candidates require another consumer's
evidence.

### 43.4 Diagnostics and acceptance evidence

Motion owners generate motion diagnostics: invalid timestamps/quaternions,
missing required bones, retarget mismatches, unsupported root policy and bad
source-rest hierarchy. The runtime forwards code, severity, subject and detail
to its structured diagnostic channel without redefining motion error codes.
New codes enter the diagnostic catalog only when implemented.

Algorithm correctness is tested here: sampling/interpolation/boundary hold,
retarget/root behavior, skeleton conversion, rest construction, malformed-input
rejection and recording/replay parity. USD tests cover resolved UsdSkel
read/write, `metersPerUnit`, `timeCodesPerSecond`, non-trivial hierarchies,
source rest, placement policy and round trips, including explicit rejection
where a policy cannot support the input.

The runtime owns cross-repository acceptance of:

```text
motionUsd -> MotionClip + source rest -> SampleClip -> Retarget
          -> runtime adapter -> EvaluatedAvatarState
```

The migration is complete when runtime adapters contain no sampling, retarget,
generic validation, USD-to-motion skeleton conversion, source-rest extraction
or recording semantics; they only marshal, invoke and publish. VRM/MMD must be
able to use the same owner APIs, and API/ABI stabilization requires evidence
from a second consumer, such as a format adapter or external runtime. No motion
component may acquire an avatar scheduler or renderer/Hydra API to achieve it.

## 44. Motion and connector boundary

**Accepted ownership policy, 2026-10-06.** `usd-motion-plugins` is the canonical
owner of generic motion semantics. `motion-connectors` reports what was
observed; this repository interprets, processes, records and converts canonical
motion. The implementation work is **Connector Boundary Phase A-D** in
[connector-boundary.md](../roadmap/connector-boundary.md), separate from
Migration Phase A-F and Runtime Boundary Phase 1-5. Candidate APIs below are
design directions, not claims that new interfaces are installed.

### 44.1 Component responsibilities

| Component | Owns | Excludes |
| --- | --- | --- |
| `motionCore` | Lowest canonical value contract: `HumanJoint`, `MotionPose`, `MotionClip`, `RootMotion`, channels, source metadata, skeleton-neutral shared values and basis arithmetic. | Network/protocol/device APIs, stage APIs, OpenExec, filtering, recording and retarget implementations. |
| `motionSampling` | Temporal semantics: sampling/status, interpolation, resampling, filtering/smoothing, blending and canonical pose buffering. | Connector-specific clock interpretation and acquisition queues. |
| `motionRecording` | `LiveCaptureSource`, semantic stream intake, canonical recording, `MotionRecorder`, `motion-capture-trace`, semantic replay and clip construction. | Raw UDP/OSC/WebSocket capture and connector sessions. |
| `motionRetarget` | Source-independent skeleton adaptation: `SkeletonDescriptor`, `RetargetMap`, rest correction, target mapping, root policy and diagnostics. | Branches on VMC, mocopi, OpenXR or other source/device names. |
| `motionUsd` | `MotionClip` ↔ `UsdSkelAnimation`, skeleton/value extraction, time-code mapping, USD authoring and target-local array authoring. | Connectors, networks, devices and source SDKs. |
| `motionSource`, `motionBvh` and profiles | Format-neutral recorded sources, BVH, declarative producer profiles and generic motion conversion. | Source-native acquisition interpretation and avatar-format semantics. |
| `execMotion` | Optional generic OpenExec motion evaluation over owner library calls. | Runtime scheduling and duplicated motion algorithms. |

These are ownership rules. The implemented library graph remains
[WORKSPACE.md §2.1](../architecture/WORKSPACE.md#21-inside-the-repository);
future API additions remain in the roadmap.

### 44.2 Canonical intake, not acquisition envelopes

`MotionFrame` remains in `motion-connectors`. It is an acquisition envelope
that can carry timestamps, sequence/source state, tracker observations,
protocol evidence and connection/session state; it is not `MotionPose`.
No motion API accepts, includes or links `MotionFrame`, `IMotionConnector` or
connector transport libraries. No reverse dependency on `motion-connectors`
is permitted.

The existing intake is `LiveCaptureSource::Push(const MotionPose&)`. Any
additional timestamp/status/value metadata must use motion-owned types. A
`MotionSampleInfo` paired with a pose, or a `MotionSample`, `MotionStreamInput`
or `MotionInputFrame`, is a candidate, not an adoption of the connector frame.
The detailed intake contract and open decision are
[MOTION_CONTRACT.md §9.1](MOTION_CONTRACT.md#91-canonical-intake-and-acquisition-envelopes).

Source restart and receive timestamps are observed by the connector. The
runtime selects intake policy, and motion APIs implement generic effects on
history, missing/stale data and discontinuities. `motionSampling` never infers
protocol epochs or reconnect behavior. Raw packet capture stays with the
connector; semantic recording and replay stay with `motionRecording`.

### 44.3 Runtime composition

The bridge between connector and motion runtime belongs outside this
repository; `usd-avatar-runtime` is the preferred composition owner:

```text
motion-connectors: MotionFrame
    -> usd-avatar-runtime: adapter / actor routing / intake policy
    -> usd-motion-plugins: MotionPose + motion-owned input metadata
    -> LiveCaptureSource -> motionSampling
```

Preparing generic intake APIs here enables external composition of
`VmcLiveSource` / `MocopiLiveSource`; it does not move those source-specific
bridges into a motion library. A runtime may depend on both packages; this
repository must remain unaware of the connector package.

### 44.4 Conditional generic tracker solve

`TrackerObservation` and the existing assignment/solve remain connector-owned
for now. Reassess tracker assignment, body solve, pose reconstruction and
confidence fusion only when the API can be explained without device/source
names, reused across OpenXR, VRChat OSC and optical mocap, and justified as
generic motion processing. Any accepted migration uses motion-owned input
values and preserves the one-way dependency; it never imports the connector
observation type. The contract and evaluation decision are
[MOTION_CONTRACT.md §11.1](MOTION_CONTRACT.md#111-a-tracker-observation-gets-no-type-here).

### 44.5 Dependency invariants and enforcement

The mandatory invariants are:

1. This repository never depends on `motion-connectors`.
2. Device, protocol, browser and network dependencies never enter motion libraries.
3. Filtering, retargeting and semantic recording exist exactly once, here.
4. `motionCore` remains usable without a stage, renderer, network or device.
5. Source-specific coordinate interpretation never enters generic motion processing.

Library graph, include and link checks must guard these rules, including
OpenXR, MediaPipe, OSC, WebSocket implementations, device SDKs and socket APIs.
The enforcement surface is owned by
[WORKSPACE.md §2.6](../architecture/WORKSPACE.md#26-connector-boundary-target);
remaining coverage is Connector Boundary Phase D. Source/protocol strings
stored as provenance do not authorize behavior branches or dependencies.
