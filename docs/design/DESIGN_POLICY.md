# usd-motion-plugins — Design and Implementation Policy

> Status: **accepted** as the repository's design policy, 2026-09-17. Nothing
> it describes is implemented yet; [reference/CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md)
> is the only document that says what is.  
> Repository: `animu-sphere/usd-motion-plugins`  
> Scope: vendor-neutral motion representation, retargeting, recording, and OpenUSD bridge  
> Primary consumers: `usd-vrm-plugins`, `usd-mmd-plugins`, `usd-avatar-runtime`, `motion-connectors`
>
> This is the canonical, long-form policy. Three focused documents own the
> detail of one area each, and **on their own area the focused document
> wins**: [MOTION_CONTRACT.md](MOTION_CONTRACT.md) (the in-memory motion
> values, units, time, root motion), [RETARGETING_POLICY.md](RETARGETING_POLICY.md)
> (skeleton descriptors, maps, rest correction, root-motion modes) and
> [USD_MAPPING.md](USD_MAPPING.md) (the authored stage). Component identities
> and dependency edges are [architecture/WORKSPACE.md](../architecture/WORKSPACE.md)'s.
>
> Section numbers are stable, so sibling repositories can cite them ("the
> motion-plugins policy §38"); they already do. A revision adds subsections or
> appends sections, and never changes what a number means. §42 records the
> decisions taken since the policy was written.

---

## 1. Project overview

`usd-motion-plugins` is the motion interoperability layer for the Animusphere/OpenStrata avatar stack.

Its purpose is to provide a format-neutral and avatar-format-neutral representation of motion, together with reusable retargeting, recording, sampling, and OpenUSD authoring facilities.

The repository sits between external motion sources and avatar-specific semantics:

```text
external world
    │
    │  mocap / WebXR / MediaPipe / VMC / files / AI / procedural systems
    ▼
motion-connectors
    │
    │  normalized live samples
    ▼
MotionPose / MotionStream
    │
    ▼
usd-motion-plugins
    │
    ├─ representation
    ├─ humanoid semantics
    ├─ sampling / interpolation
    ├─ retargeting
    ├─ recording
    └─ OpenUSD bridge
    │
    ▼
usd-vrm-plugins / usd-mmd-plugins / future avatar-format plugins
    │
    │  avatar-format semantics and target-specific binding
    ▼
usd-avatar-runtime
    │
    └─ runtime composition / OpenExec / application integration
```

The central rule is:

> **Motion is represented independently of its transport, source product, avatar format, and runtime host.**

`usd-motion-plugins` must not become a second VRM repository, a second MMD repository, or a collection of vendor SDK adapters.

---

## 2. Goals

The repository should make the following paths possible through one shared motion model:

```text
BVH / VMD / VRMA / NPZ
          │
          ▼
      MotionClip
          │
          ├───────────────┐
          ▼               ▼
      Retarget        USD authoring
          │               │
          ▼               ▼
     target rig      UsdSkelAnimation
```

and:

```text
WebXR / MediaPipe / VMC / mocap
              │
              ▼
        motion-connectors
              │
              ▼
          MotionStream
              │
        ┌─────┴─────┐
        ▼           ▼
     Retarget    Recorder
        │           │
        ▼           ▼
   live target   MotionClip
```

and eventually:

```text
AI / procedural generator
          │
          ▼
       MotionClip
          │
          ▼
    same downstream path
```

The implementation should optimize for:

- stable semantic contracts;
- standard OpenUSD representation wherever possible;
- deterministic offline conversion;
- low-overhead realtime sampling;
- explicit coordinate-space and time semantics;
- reusable retargeting independent of VRM or MMD;
- testability without requiring a renderer or application runtime;
- future composition into OpenStrata runtimes.

---

## 3. Non-goals

The following are intentionally outside the core responsibility of this repository.

### 3.1 Device and protocol connectivity

Do not place protocol clients or device SDKs in the motion core.

Examples:

```text
MediaPipe
WebXR
VMC / OSC
mocopi
camera tracking
IMU devices
VR trackers
vendor SDKs
network transports
```

These belong in `motion-connectors` or optional integration packages.

### 3.2 Avatar-format semantics

Do not make the core depend on:

```text
VRM humanoid metadata
VRM expressions
VRM look-at
VRM spring bones

MMD PMX bone flags
MMD morph semantics
MMD IK conventions
MMD physics conventions
```

Those remain owned by their format repositories.

`usd-motion-plugins` may provide generic mechanisms that those repositories consume.

### 3.3 Runtime orchestration

The core does not own:

- application update loops;
- game-loop scheduling;
- OpenExec graph ownership;
- network session management;
- physics world ownership;
- character-controller behavior;
- camera behavior.

Those belong to `usd-avatar-runtime`, `usd-stage-runner`, or a future runtime package.

### 3.4 Rendering

The repository does not render avatars or motion.

Hydra delegates, toon shading, MToon, materials, and viewport behavior are outside scope.

---

## 4. Design principles

### 4.1 Vendor-neutral core

Core types must never require a specific provider.

Forbidden:

```cpp
if (provider == "mocopi") {
    ...
}

if (sourceProtocol == "VMC") {
    ...
}
```

Allowed:

```cpp
sample.metadata.provider = "mocopi";
sample.metadata.protocol = "vmc";
```

Provider metadata is diagnostic/provenance information, not behavioral control flow.

If one provider exposes a capability that matters generally, promote that capability into the generic model.

### 4.2 Avatar-format-neutral core

Retargeting is based on semantic joints, skeleton topology, rest pose, coordinate spaces, and policy.

It must not require a VRM or PMX object.

Preferred:

```cpp
Retarget(
    const MotionPose& source,
    const SkeletonDescriptor& target,
    const RetargetMap& map,
    const RetargetPolicy& policy);
```

Avoid:

```cpp
RetargetToVrm(...)
RetargetToPmx(...)
```

Format repositories are responsible for constructing the generic descriptors.

### 4.3 Existing OpenUSD schemas first

When motion becomes USD, use standard schemas wherever they fit.

The primary body-animation representation is:

```text
UsdSkelAnimation
```

Skeleton binding remains based on:

```text
UsdSkelSkeleton
UsdSkelBindingAPI
```

A project-specific schema should be introduced only when a stable concept cannot be represented cleanly with existing OpenUSD schemas, namespaced metadata, or ordinary composition.

### 4.4 Representation and evaluation are separate

A motion clip is data.

Applying it to a particular character is evaluation.

Keep:

```text
source motion
target skeleton
binding
retarget policy
evaluated result
```

as distinct concepts.

### 4.5 Offline and realtime share the same semantic model

Recorded motion and live motion should differ primarily in storage/lifetime, not meaning.

```text
MotionPose       one sample
MotionStream     sequence arriving over time
MotionClip       finite stored sequence
```

A recorder converts:

```text
MotionStream -> MotionClip
```

A clip source can expose:

```text
MotionClip -> sampled MotionPose
```

This symmetry is intentional.

---

## 5. Core semantic model

## 5.1 MotionPose

`MotionPose` is the canonical instantaneous body-motion representation.

Recommended shape:

```cpp
struct MotionPose {
    double timestamp = 0.0;

    RootMotion root;

    std::array<GfQuatf, HumanJointCount> localRotations;
    std::bitset<HumanJointCount> validRotations;

    std::optional<std::array<GfVec3f, HumanJointCount>> localTranslations;
    std::bitset<HumanJointCount> validTranslations;

    std::optional<std::array<float, HumanJointCount>> confidence;

    MotionChannelSet channels;
    SourceMetadata metadata;
};
```

Important invariants:

- timestamps are expressed in seconds;
- body joint rotations are local-space rotations;
- root/world motion is not encoded implicitly into hips;
- missing joints are explicitly representable;
- confidence is optional;
- translations are optional rather than assumed for every joint;
- input source identity does not change the semantic meaning of the pose.

### 5.1.1 Why root motion is separate

Do not conflate:

```text
character world transform
```

with:

```text
hips local transform
```

They have different composition and retargeting behavior.

Recommended:

```cpp
struct RootMotion {
    GfVec3f position;
    GfQuatf orientation;

    GfVec3f linearVelocity;
    GfVec3f angularVelocity;

    bool hasPosition = false;
    bool hasOrientation = false;
    bool hasLinearVelocity = false;
    bool hasAngularVelocity = false;
};
```

---

## 5.2 Human joint semantics

The first canonical semantic layer should be a compact humanoid joint vocabulary.

Example:

```text
hips
spine
chest
upperChest
neck
head

leftShoulder
leftUpperArm
leftLowerArm
leftHand

rightShoulder
rightUpperArm
rightLowerArm
rightHand

leftUpperLeg
leftLowerLeg
leftFoot
leftToes

rightUpperLeg
rightLowerLeg
rightFoot
rightToes
```

The exact set must be versioned and documented.

The canonical joint vocabulary is **not** a USD prim path list and is **not** a VRM-specific bone list.

A target avatar maps its real joints onto the canonical semantics.

```text
canonical semantic
        │
        ▼
    RetargetMap
        │
        ▼
target skeleton joint
```

Optional joints must remain optional.

The implementation must not assume every source or target has identical topology.

---

## 5.3 MotionChannelSet

Body joints are not the only time-varying values used by avatar systems.

However, `usd-motion-plugins` should not prematurely standardize VRM expressions and MMD morphs into one avatar-specific vocabulary.

Provide a generic channel mechanism:

```cpp
struct MotionChannel {
    TfToken semantic;
    VtValue value;
    float confidence = 1.0f;
};

using MotionChannelSet = std::vector<MotionChannel>;
```

Possible future generic semantics include:

```text
face/blinkLeft
face/blinkRight
face/jawOpen
gaze/target
contact/leftFoot
contact/rightFoot
```

Format-specific channels may be carried as namespaced semantics:

```text
vrm:happy
mmd:あ
arkit:jawOpen
```

The core must preserve such values without interpreting avatar-format-specific meaning.

A future standardization effort can promote proven common channels into canonical semantics without breaking transport.

---

## 5.4 SourceMetadata

Provenance should survive normalization without contaminating behavior.

```cpp
struct SourceMetadata {
    std::string provider;
    std::string protocol;
    std::string sourceId;

    std::optional<double> sourceTimestamp;
    std::optional<uint64_t> sequenceNumber;
};
```

Additional arbitrary metadata may be supported through a namespaced dictionary.

---

## 6. MotionStream

`MotionStream` represents a realtime or incrementally produced sequence of `MotionPose` samples.

The core contract should not require networking.

Recommended API direction:

```cpp
class IMotionStream {
public:
    virtual ~IMotionStream() = default;

    virtual bool TryRead(MotionPose* pose) = 0;
};
```

or a push form:

```cpp
using MotionSink = std::function<void(const MotionPose&)>;

class IMotionProducer {
public:
    virtual ~IMotionProducer() = default;
    virtual void SetSink(MotionSink sink) = 0;
};
```

Do not hard-code one concurrency model into the semantic package.

The first implementation should prefer a simple synchronous library API and allow runtime packages to wrap it with threads, tasks, sockets, or OpenExec.

---

## 7. MotionClip

A finite motion sequence is represented by `MotionClip`.

Recommended model:

```cpp
struct MotionClip {
    std::vector<MotionPose> samples;

    double startTime = 0.0;
    double endTime = 0.0;
    double nominalFrameRate = 0.0;

    ClipMetadata metadata;
};
```

The stored sample timestamps remain authoritative.

`nominalFrameRate` is descriptive and must not replace actual time values.

A clip may be:

- uniformly sampled;
- variably sampled;
- sparse;
- partially populated;
- generated from a file;
- recorded from a stream;
- produced procedurally;
- produced by a learned model.

Downstream code should not need to know which.

---

## 8. Sampling and interpolation

Provide format-independent sampling in a common library.

Suggested API:

```cpp
MotionPose Sample(
    const MotionClip& clip,
    double time,
    const SamplingPolicy& policy);
```

`SamplingPolicy` should control:

```text
clamp / loop
nearest / interpolate
root-motion handling
missing-channel behavior
```

Default interpolation:

```text
translation -> linear interpolation
rotation    -> shortest-path normalized quaternion interpolation
scalar      -> linear interpolation
discrete    -> nearest / held sample
```

Do not embed frame-rate-specific assumptions into the sampler.

---

## 9. Coordinate system contract

Coordinate ambiguity must be resolved at repository boundaries.

Every imported or connected source must normalize into the canonical motion coordinate contract before entering shared retargeting.

Recommended canonical contract:

```text
handedness:      OpenUSD-compatible right-handed interpretation
up axis:         Y
linear unit:     meter
time unit:       second
rotation:        quaternion
joint rotation:  local space
root transform:  character/world relationship, explicit
```

Do not retain source-unit ambiguity inside `MotionPose`.

Conversion responsibility:

```text
external protocol/file
        │
        ▼
adapter / decoder
        │  axis + unit normalization
        ▼
canonical MotionPose
```

The original source convention should remain available through provenance metadata when useful for diagnostics.

---

## 10. SkeletonDescriptor

Retargeting requires a generic description of the target skeleton.

```cpp
struct SkeletonDescriptor {
    std::vector<TfToken> jointNames;
    std::vector<int> parentIndices;

    std::vector<GfMatrix4d> restTransforms;
    std::vector<GfMatrix4d> bindTransforms;

    std::unordered_map<HumanJoint, int> humanJointMap;
};
```

`jointNames` may preserve Unicode source names.

Do not require source names to be valid USD identifiers at this layer.

USD path sanitization is a separate authoring concern.

---

## 11. RetargetMap

`RetargetMap` expresses semantic correspondence.

```cpp
struct RetargetMap {
    std::array<int, HumanJointCount> targetJointIndices;
    std::bitset<HumanJointCount> validTargets;
};
```

A richer implementation may include:

```text
source rest orientation
target rest orientation
rotation offsets
translation scale policy
twist distribution
optional chain definitions
joint limits
```

The map should be buildable from:

- VRM humanoid semantics;
- MMD bone-role heuristics or explicit mapping;
- manually authored mapping;
- future standardized character schemas.

The retarget core must not care which produced it.

---

## 12. Retargeting pipeline

Preferred pipeline:

```text
source MotionPose
      │
      ▼
canonical semantic joints
      │
      ▼
rest-pose normalization
      │
      ▼
RetargetMap
      │
      ▼
target-space joint rotations
      │
      ├─ optional chain/twist correction
      ├─ optional limb scaling
      └─ root-motion policy
      ▼
RetargetedPose
```

Retargeting should be deterministic for identical inputs and policy.

### 12.1 Initial scope

The first production implementation should support:

- semantic rotation retargeting;
- different joint ordering;
- different rest orientations;
- missing optional joints;
- configurable root-motion behavior.

### 12.2 Later scope

Add only after the basic contract is stable:

- limb-length-aware positional retargeting;
- IK-assisted feet/hands;
- foot locking;
- contact constraints;
- twist-bone distribution;
- joint-limit enforcement;
- locomotion warping;
- trajectory constraints.

These features belong in generic retargeting only when their APIs remain avatar-format-neutral.

---

## 13. Root-motion policy

Root motion must be explicit.

Recommended modes:

```text
Preserve
Remove
Extract
InPlace
ProjectToGround
```

Example policy:

```cpp
struct RootMotionPolicy {
    RootMotionMode mode;
    bool preserveVerticalMotion = true;
    bool preserveYaw = true;
};
```

Do not silently transform root motion as a side effect of retargeting.

---

## 14. Recording

Recording converts a `MotionStream` into a persistent `MotionClip`.

```text
MotionStream
    │
    ▼
MotionRecorder
    │
    ├─ timestamp normalization
    ├─ optional resampling
    ├─ optional filtering
    └─ provenance capture
    ▼
MotionClip
```

Recommended API direction:

```cpp
class MotionRecorder {
public:
    void Begin(const RecordingOptions&);
    void Push(const MotionPose&);
    MotionClip End();
};
```

Recording should not itself write VRMA, VMD, BVH, or another external format.

Format-specific exporters, when implemented, belong to their corresponding format plugin/repository.

---

## 15. Filtering and stabilization

Realtime capture often requires smoothing, but filtering must be explicit and composable.

Possible processors:

```text
QuaternionSmoothingFilter
PositionSmoothingFilter
ConfidenceGate
DeadReckoningFilter
GapFiller
FootContactDetector
```

Preferred shape:

```cpp
class IMotionProcessor {
public:
    virtual ~IMotionProcessor() = default;
    virtual MotionPose Process(const MotionPose&) = 0;
};
```

A processor chain can then be composed:

```text
stream
  -> confidence gate
  -> smoothing
  -> retarget
  -> recorder/runtime
```

Do not bake one smoothing algorithm into the definition of `MotionStream`.

---

## 16. OpenUSD bridge

The OpenUSD bridge converts between canonical motion data and standard USD animation constructs.

Suggested library:

```text
libs/motion-usd
```

Responsibilities:

```text
MotionClip -> UsdSkelAnimation
UsdSkelAnimation -> MotionClip
SkeletonDescriptor <-> UsdSkelSkeleton
time-code conversion
joint-order mapping
metadata/provenance authoring
```

The bridge is not a file-format plugin by itself.

---

## 17. USD authoring policy

### 17.1 Root prim

For a standalone motion asset, use:

```text
/Animation
```

as the default prim.

Recommended stage:

```text
/Animation
├─ Skeleton
├─ Body
├─ Channels
└─ Metadata
```

However, the primary semantic body animation should remain standard USD:

```text
/Animation/Body
    -> UsdSkelAnimation
```

`/Animation/Skeleton` may represent a canonical semantic skeleton when needed for standalone inspection or interchange.

### 17.2 Stage metrics

Default policy:

```text
upAxis = Y
metersPerUnit = 1
timeCodesPerSecond = source rate when meaningful, otherwise a documented default
```

Actual sample times remain semantically authoritative.

### 17.3 Unicode

Source bone/channel names may contain Japanese or other Unicode text.

Preserve original names in attributes/metadata.

When a USD prim path or identifier requires normalization:

```text
original display/source name
        │
        ├─ preserved as metadata
        ▼
deterministic safe identifier
```

Never discard the original name after sanitization.

### 17.4 Metadata

Recommended custom data namespace:

```text
customData.motion
```

Possible fields:

```text
sourceFormat
sourceProvider
duration
sampleCount
nominalFrameRate
semanticVersion
```

Keep runtime-only transient state out of authored asset metadata.

---

## 18. USD composition model

Do not make a source motion clip directly own a target avatar.

Preferred scene assembly:

```text
/World
├─ Character
│   └─ references avatar asset
│
├─ Motions
│   └─ Walk
│       └─ references motion asset
│
└─ Bindings
    └─ CharacterWalk
        ├─ target = </World/Character>
        ├─ source = </World/Motions/Walk>
        └─ retarget policy
```

This preserves:

```text
avatar asset
motion asset
application relationship
```

as separate composition concerns.

A baked target-specific `UsdSkelAnimation` may be authored under the character or in a derived layer, but it is not the same asset as the original source motion.

---

## 19. Avatar repository integration

## 19.1 usd-vrm-plugins

`usd-vrm-plugins` owns:

- `.vrm` semantics;
- `.vrma` decoding/format semantics if retained there;
- VRM humanoid mapping;
- VRM expression semantics;
- VRM look-at semantics;
- VRM-specific avatar metadata.

It should consume shared functionality from `usd-motion-plugins` for:

```text
MotionPose
MotionClip
sampling
retargeting
recording
UsdSkel bridge
```

Long-term, generic motion code currently located in the VRM repository should migrate here.

### 19.2 usd-mmd-plugins

`usd-mmd-plugins` owns:

- PMX/PMD avatar semantics;
- VMD file-format specifics when the format package is kept there;
- MMD bone/morph/IK semantics;
- MMD-specific naming and compatibility rules.

It should normalize reusable body motion into the same motion core.

Example:

```text
VMD decoder
    │
    ├─ MMD-specific morph tracks -> usd-mmd-plugins semantics
    │
    └─ body transforms
            │
            ▼
        MotionClip
```

### 19.3 Avoid circular dependencies

Required dependency direction:

```text
usd-motion-plugins
        ▲
        │
        ├─ usd-vrm-plugins
        └─ usd-mmd-plugins
```

Never:

```text
usd-motion-plugins -> usd-vrm-plugins
usd-motion-plugins -> usd-mmd-plugins
```

---

## 20. motion-connectors integration

`motion-connectors` owns the external world boundary.

Examples:

```text
connectors/
├─ webxr/
├─ mediapipe/
├─ vmc/
├─ mocopi/
└─ custom-osc/
```

Its job ends when data has been normalized into:

```text
MotionPose
```

or:

```text
MotionStream
```

The connector may depend on `motion-core`.

`motion-core` must not depend on connector implementations.

---

## 21. OpenExec boundary

OpenExec belongs to evaluation/orchestration, not the passive motion data model.

Therefore:

```text
libs/motion-core
libs/motion-retarget
libs/motion-usd
```

must build and operate without OpenExec.

OpenExec integration should be an optional bundle:

```text
plugins/execMotion/
```

or, if runtime ownership becomes clearer:

```text
usd-avatar-runtime
    └─ execMotion
```

The OpenExec layer may expose operations such as:

```text
SampleMotion
BlendMotion
RetargetPose
ApplyRootMotion
ResolveMotionChannels
RecordMotion
```

but those nodes should call the same ordinary C++ libraries used by offline tools.

The evaluation graph is an adapter over the motion libraries, not their implementation home.

---

## 22. Proposed repository structure

Recommended initial workspace:

```text
usd-motion-plugins/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ VERSION
├─ LICENSE
├─ README.md
├─ openstrata.toml
├─ openstrata.ci.yaml
│
├─ cmake/
│
├─ libs/
│  ├─ motion-core/
│  │  ├─ include/
│  │  ├─ src/
│  │  └─ tests/
│  │
│  ├─ motion-sampling/
│  │  ├─ include/
│  │  ├─ src/
│  │  └─ tests/
│  │
│  ├─ motion-retarget/
│  │  ├─ include/
│  │  ├─ src/
│  │  └─ tests/
│  │
│  ├─ motion-recording/
│  │  ├─ include/
│  │  ├─ src/
│  │  └─ tests/
│  │
│  └─ motion-usd/
│     ├─ include/
│     ├─ src/
│     └─ tests/
│
├─ plugins/
│  └─ execMotion/              # optional / later
│
├─ tools/
│  ├─ motion-inspect/
│  ├─ motion-convert/
│  └─ motion-record/           # optional / later
│
├─ python/
│  └─ usd_motion/              # later
│
├─ tests/
│  ├─ corpus/
│  ├─ golden/
│  └─ integration/
│
└─ docs/
   ├─ architecture/
   ├─ design/
   │  ├─ DESIGN_POLICY.md
   │  ├─ MOTION_CONTRACT.md
   │  ├─ RETARGETING_POLICY.md
   │  └─ USD_MAPPING.md
   ├─ reference/
   └─ roadmap/
```

Do not start with every directory populated.

The first release should keep the number of independently versioned targets small.

---

## 23. C++ namespace policy

Recommended public namespace:

```cpp
openstrata::motion
```

Optional subnamespaces:

```cpp
openstrata::motion::sampling
openstrata::motion::retarget
openstrata::motion::recording
openstrata::motion::usd
```

Do not encode source formats into the core namespace.

---

## 24. Dependency policy

Keep the core dependency graph narrow.

Preferred:

```text
motion-core
    │
    ├─ OpenUSD foundation types where justified
    └─ standard library

motion-sampling
    └─ motion-core

motion-retarget
    └─ motion-core

motion-recording
    ├─ motion-core
    └─ motion-sampling

motion-usd
    ├─ motion-core
    ├─ motion-sampling
    └─ OpenUSD UsdSkel
```

Avoid introducing heavy ML, networking, UI, rendering, or device dependencies into shared libraries.

---

## 25. Python API

Python bindings are useful for inspection, conversion, dataset processing, testing, and AI workflows.

Target shape:

```python
import usd_motion as motion

clip = motion.load(...)
pose = clip.sample(1.25)

result = motion.retarget(
    pose,
    target=skeleton,
    mapping=mapping,
)
```

But Python should wrap the C++ semantic implementation rather than become a second implementation.

If packaging is introduced, prefer a stable ABI strategy where practical and keep OpenUSD ABI compatibility explicit.

Python bindings are not required for the earliest C++ vertical slice.

---

## 26. File-format ownership

`usd-motion-plugins` should not automatically absorb every animation file format.

A format belongs here when it is fundamentally a generic motion format rather than an avatar-format-owned extension.

Candidate ownership:

```text
BVH
    -> strong candidate for usd-motion-plugins

generic motion NPZ
    -> candidate, after a contract is defined

VRMA
    -> usd-vrm-plugins

VMD
    -> usd-mmd-plugins
```

This keeps semantic ownership clear.

A future repository layout could therefore contain:

```text
plugins/
└─ motion-bvh/
```

while VRMA and VMD remain consumers/producers of the shared core from their own repositories.

---

## 27. BVH direction

BVH is a good first generic file-format integration after the core libraries exist because it exercises:

- skeleton hierarchy;
- named joints;
- local rotations;
- translations;
- time sampling;
- coordinate conversion;
- `UsdSkelAnimation` authoring.

Recommended path:

```text
.bvh
  -> BvhDocumentReader
  -> generic skeleton + MotionClip
  -> motion-usd
  -> /Animation
```

The BVH parser must remain separate from USD authoring.

---

## 28. Generic NPZ direction

Do not define “NPZ support” as “accept arbitrary NumPy archives.”

First define a versioned motion payload contract.

Example conceptual fields:

```text
version
timestamps
joint_semantics
rotations
translations
root_position
root_rotation
confidence
metadata
```

Only then add an NPZ reader/writer.

The container is not the schema.

---

## 29. Diagnostics

Use stable diagnostic codes for externally visible failures.

Suggested prefixes:

```text
MOTION-E####   error
MOTION-W####   warning
MOTION-I####   informational
```

Subcomponents may receive narrower prefixes later.

Diagnostics should distinguish:

```text
malformed data
unsupported valid data
missing optional data
mapping failure
retarget policy conflict
USD authoring failure
```

Do not collapse all failures into generic “invalid motion”.

---

## 30. Testing policy

Tests should be deterministic and layered.

### 30.1 motion-core

Test:

- missing joints;
- quaternion normalization;
- timestamps;
- metadata preservation;
- root/body separation;
- Unicode metadata.

### 30.2 sampling

Test:

- exact key sampling;
- interpolation;
- looping;
- clamping;
- sparse channels;
- variable sample spacing.

### 30.3 retargeting

Use tiny synthetic skeletons with known answers.

Test:

- same skeleton identity mapping;
- different joint order;
- rest-pose offset;
- missing optional bones;
- root-motion modes;
- scale/proportion differences as support expands.

### 30.4 recording

Test:

- monotonic timestamps;
- stream-to-clip conversion;
- optional resampling;
- dropped/missing samples;
- metadata preservation.

### 30.5 USD bridge

Golden tests should verify:

- prim hierarchy;
- joint ordering;
- `UsdSkelAnimation` values;
- time samples;
- default prim;
- stage metrics;
- round-trip tolerance where supported.

### 30.6 Cross-repository integration

Integration tests should prove paths such as:

```text
VRMA -> MotionClip -> target VRM
VMD  -> MotionClip -> target PMX
BVH  -> MotionClip -> VRM target
live MotionPose -> recorder -> USD
```

These tests may live in runtime/integration repositories to avoid dependency cycles.

---

## 31. Performance policy

Correctness comes first, but core APIs should not preclude realtime operation.

Avoid:

- unnecessary per-frame heap allocation;
- repeated string lookup for canonical joints;
- repeated skeleton-map construction;
- repeated USD stage traversal inside per-sample retargeting.

Prefer precomputed state:

```text
RetargetMap
SkeletonDescriptor
sampling cursor/cache
resolved channel bindings
```

A realtime evaluator should be able to reuse these objects across frames.

Benchmark only after the first vertical slice is correct.

---

## 32. Threading policy

The semantic libraries should be thread-compatible but should not own a global worker pool.

Avoid embedding runtime scheduling into the core.

Objects with mutable realtime state, such as stream buffers or filters, must document their thread-safety explicitly.

Immutable descriptors and clips should be easy to share.

---

## 33. Serialization stability

Public in-memory types and persistent interchange contracts are different things.

Do not guarantee that the raw C++ layout of `MotionPose` is a stable serialized ABI.

Persistent formats must have explicit versions.

Examples:

```text
USD mapping version
NPZ contract version
network connector protocol version
```

Version migration should occur at boundaries.

---

## 34. Security and robustness

Motion inputs can be untrusted.

Readers and bridges should validate:

- unreasonable sample counts;
- non-finite floats;
- invalid quaternion values;
- non-monotonic timestamps where prohibited;
- integer overflow;
- excessively deep skeletons;
- duplicate semantic mappings;
- invalid parent indices.

Realtime adapters must also bound queues and reject unbounded growth.

---

## 35. Release strategy

### v0.1.0 — core contract

Deliver the smallest complete path:

```text
MotionPose
MotionClip
HumanJoint semantics
SkeletonDescriptor
basic sampler
basic retarget map
UsdSkelAnimation authoring
deterministic tests
```

No networking.

No provider adapters.

No OpenExec requirement.

No complex IK.

### v0.2.0 — retargeting

Add:

```text
rest-pose-aware rotation retarget
root-motion policy
mapping validation
VRM/MMD integration hooks
```

### v0.3.0 — recording and stream utilities

Add:

```text
MotionStream contract
MotionRecorder
filter/processor interface
stream-to-clip tests
```

### v0.4.0 — generic format integration

Preferred:

```text
BVH reader
BVH -> MotionClip -> USD
```

### v0.5.0 — runtime integration

Evaluate:

```text
execMotion
OpenExec nodes
usd-avatar-runtime composition
```

only after ordinary library APIs are stable.

### Later

Potential areas:

```text
generic NPZ contract
IK-assisted retargeting
contact constraints
motion blending
trajectory warping
AI generation interfaces
Python package
WASM-facing API
```

---

## 36. Initial vertical slice

The first implementation should prove one narrow end-to-end contract before expanding.

Recommended order:

```text
workspace scaffold
    ↓
motion-core target
    ↓
HumanJoint + MotionPose + MotionClip
    ↓
synthetic test clip
    ↓
motion-usd target
    ↓
MotionClip -> UsdSkelAnimation
    ↓
stage opens through OpenUSD
    ↓
motion-retarget target
    ↓
identity retarget
    ↓
rest-pose-offset retarget
    ↓
integration fixture
```

Only after this works should BVH, live streams, OpenExec, or Python be added.

---

## 37. Migration from usd-vrm-plugins

Existing generic motion code should move gradually rather than through a flag day.

Recommended migration:

```text
Phase A
    define usd-motion-plugins public contracts

Phase B
    make usd-vrm-plugins adapt its existing data to MotionPose / MotionClip

Phase C
    move generic sampling and retargeting implementations

Phase D
    make usd-mmd-plugins consume the same core

Phase E
    extract generic recording / streaming support

Phase F
    reduce duplicated legacy motion code in avatar-format repositories
```

During migration, adapter code inside `usd-vrm-plugins` is acceptable.

Duplicating a generic algorithm permanently is not.

---

## 38. Decision rules for repository placement

When adding a feature, ask:

### Put it in `usd-motion-plugins` when:

- it applies to motion independent of VRM/MMD;
- it consumes or produces `MotionPose` / `MotionClip`;
- it operates on generic skeleton descriptors;
- it is reusable offline and realtime;
- it maps generic motion to/from standard USD.

### Put it in `motion-connectors` when:

- it talks to an external device/service/process;
- it decodes a live transport/protocol;
- it owns network reconnection or device lifecycle.

### Put it in `usd-vrm-plugins` when:

- it requires VRM specification semantics;
- it resolves VRM expressions/look-at/spring-bone behavior;
- it reads/writes VRM/VRMA-specific structures.

### Put it in `usd-mmd-plugins` when:

- it requires PMX/PMD/VMD-specific semantics;
- it depends on MMD bone flags, IK conventions, morph categories, or physics meaning.

### Put it in `usd-avatar-runtime` when:

- it schedules or composes runtime evaluation;
- it wires motion, avatar semantics, physics, and application state together;
- it owns OpenExec execution policy.

---

## 39. Architectural invariants

The following should be treated as non-negotiable unless an ADR explicitly changes them.

1. `usd-motion-plugins` never depends on `usd-vrm-plugins`.
2. `usd-motion-plugins` never depends on `usd-mmd-plugins`.
3. Device/vendor names never control core behavior.
4. Root motion and hips-local motion remain distinct.
5. Time is represented in seconds in the canonical core.
6. Missing joints are valid representable state.
7. Retargeting consumes generic skeleton/semantic data.
8. OpenUSD authoring prefers standard `UsdSkel` schemas.
9. OpenExec is optional and layered above ordinary C++ motion libraries.
10. Recording produces generic motion first, not an avatar-format file.
11. Source Unicode names are preserved even when USD-safe identifiers are generated.
12. Offline clips and realtime streams share the same pose semantics.

---

## 40. Definition of success

`usd-motion-plugins` is successful when:

- a motion sample can arrive from an arbitrary connector without vendor logic entering the core;
- the same `MotionPose` can be applied to VRM, MMD, and future avatar targets through generic retargeting;
- a finite motion can be represented as `MotionClip`, sampled, recorded, and authored as `UsdSkelAnimation`;
- source and target skeleton differences are handled through explicit mappings and policies;
- format repositories retain ownership of their own semantics without duplicating generic motion algorithms;
- runtime systems can consume the libraries without the libraries depending on a runtime;
- adding a new motion source does not require changes to retargeting;
- adding a new avatar format does not require changes to motion source adapters.

---

## 41. Policy summary

```text
Repository:
    animu-sphere/usd-motion-plugins

Core public concepts:
    MotionPose
    MotionStream
    MotionClip
    HumanJoint
    SkeletonDescriptor
    RetargetMap
    RootMotion

Primary responsibilities:
    representation
    humanoid semantics
    sampling
    retargeting
    recording
    OpenUSD bridge

Primary USD schema:
    UsdSkelAnimation

Standalone USD root:
    /Animation

Canonical units:
    meter
    second
    Y-up
    explicit local joint rotations
    explicit root motion

Does not own:
    device connectivity
    protocol transports
    VRM semantics
    MMD semantics
    physics runtime
    rendering
    application loop

Dependency direction:
    motion-connectors      -> usd-motion-plugins
    usd-vrm-plugins        -> usd-motion-plugins
    usd-mmd-plugins        -> usd-motion-plugins
    usd-avatar-runtime     -> all of the above

Initial implementation:
    MotionPose / MotionClip
        -> sampler
        -> generic retarget
        -> UsdSkelAnimation
```

---

## 42. Decisions recorded since adoption

§42.1–§42.6 were taken on 2026-09-17, while `usd-mmd-plugins` and
`usd-vrm-plugins` aligned their documentation with this policy. §42.7 was taken
on 2026-09-19, with the scaffold, and §42.8 the same day, with the first
imported diagnostic. Each is recorded where it is binding, and
this section is the index.

### 42.1 The core is imported from `usd-vrm-plugins`, not rewritten

`usd-vrm-plugins` already implements most of §5–§16 — `motionCore`,
`motionRuntime`, a rest-pose-aware retargeter, a BVH reader behind a
format-neutral source layer, a capture recorder and vendor-neutral OpenExec
nodes — with measured behaviour and parity evidence. The code moves here with
its history, one identity at a time, in the dependency order that repository's
workspace contract fixes (its WORKSPACE.md §9); device and protocol input goes
to `motion-connectors`. What this means for §36's vertical slice is in
[roadmap/README.md](../roadmap/README.md).

### 42.2 Names are this policy's, applied on arrival

| `usd-vrm-plugins` today | Here |
| --- | --- |
| namespace `motion` | `openstrata::motion` |
| `HumanoidPose` | `MotionPose` |
| `HumanoidAnimation` | `MotionClip` |
| `HumanBone` | `HumanJoint` |
| `RootMotion` | `RootMotion` |
| `vrmRetarget::TargetSkeleton` | `SkeletonDescriptor` |
| the humanoid map | `RetargetMap` |
| `ExpressionWeights` on the pose | `MotionChannelSet` ([MOTION_CONTRACT.md §6](MOTION_CONTRACT.md#6-channels)) |

### 42.3 Migration phases are always qualified

§37's phases are written **Migration Phase A–F** everywhere, because
`usd-vrm-plugins` has an unrelated Motion Phase A–H and `usd-mmd-plugins` a
Phase 0–9.

### 42.4 A format repository evaluates its own control semantics first

§19.2 shows a VMD decoder handing body transforms straight to a `MotionClip`.
For MMD that is not possible: VMD bone tracks drive a control rig, and leg
motion lives on IK bones. `usd-mmd-plugins` therefore evaluates IK and append
transforms in its own plain library (`mmdControl`) and hands this repository
only evaluated deformation-joint motion. The general rule: **a `MotionClip`
always holds body motion**; evaluating a format's own rig to get there is the
format repository's semantics (§38), never this repository's.

### 42.5 The canonical basis includes a forward axis

§9 names handedness, up axis and units. The forward axis is **+Z**, as
`usd-vrm-plugins` measured and recorded: the avatars motion is retargeted onto
face +Z by their own specification, and a VMC sender's forward is canonical's
([MOTION_CONTRACT.md §3](MOTION_CONTRACT.md#3-coordinates-and-units)).

### 42.6 Evidence travels with the code

The focused contracts start from what `usd-vrm-plugins` measured — the basis,
the hips-as-root record, the rest-pose path rule, the retarget formula, the
`scales` requirement of `UsdSkelAnimation`, the OpenExec findings — and cite
it. They are proposals only where this policy and that evidence disagree, and
each disagreement is an open question in the focused document, not a silent
choice.

### 42.7 Identities are lower-camel, as in the siblings

§22 draws the tree with kebab-case directories (`libs/motion-core/`), and §23's
namespace does not say what the CMake names are. The scaffold follows the
workspace discipline `usd-vrm-plugins` and `usd-mmd-plugins` already share
instead. A library's identity is lower-camel (`motionCore`), and that one name
is its directory, its CMake package and its exported target
(`motionCore::motionCore`). A CLI's command is snake_case (`motion_convert`).
So the libraries §22 lists are `motionCore`, `motionSampling`,
`motionRetarget`, `motionRecording` and `motionUsd`, and its tools are
`motion_inspect`, `motion_convert` and `motion_record`. The code arrives under
the names it already builds with, and `usd-avatar-runtime` composes packages
that all follow one rule. Binding in
[WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-bundles-tools-and-data).

### 42.8 Diagnostic codes are named, and renamed on arrival

§29 proposes numbered codes (`MOTION-E0001`). The code that arrives and both
sibling repositories use named codes whose name is the event
(`VRM_RETARGET_UNBOUND_DRIVEN_BONE`, `MMD_MOTION_UNMATCHED_BONE`), and a test
that asserts a name reads as what happened. So codes here are named,
`MOTION_<AREA>_<EVENT>`, and an imported code takes this repository's prefix
on arrival under §42.2, with its event name unchanged: `motionBvh`'s
`VRM_BVH_*` are `MOTION_BVH_*`. A consumer's own codes still pass through
unchanged. Binding in
[DIAGNOSTICS.md](../reference/DIAGNOSTICS.md) (DIAG-O1).
