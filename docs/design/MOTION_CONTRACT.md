# Motion contract

> Status: **proposed**, 2026-09-17. Nothing here is implemented in this
> repository yet. Almost all of it *is* implemented, tested and measured in
> `usd-vrm-plugins` (its `libs/motionCore`, `libs/motionRuntime` and its own
> `MOTION_CONTRACT.md`), and arrives here with that code
> ([DESIGN_POLICY.md §42.1](DESIGN_POLICY.md#421-the-core-is-imported-from-usd-vrm-plugins-not-rewritten)).
> A section becomes **binding** when the code it describes lands here with its
> tests; the capability matrix says when.
>
> This document owns the in-memory motion values: joint vocabulary, units,
> basis, time, the pose, root motion, channels, provenance, clips and streams.
> On that area it wins over [DESIGN_POLICY.md](DESIGN_POLICY.md) §5–§9.
> Retargeting is [RETARGETING_POLICY.md](RETARGETING_POLICY.md)'s; the authored
> stage is [USD_MAPPING.md](USD_MAPPING.md)'s. Section numbers are stable; open
> questions are `MC-O<n>` and never reused.
>
> Where a rule below says *measured*, the measurement is `usd-vrm-plugins`',
> and the rule is carried rather than re-derived.

---

## 1. Scope

- **In:** `HumanJoint`, `MotionPose`, `RootMotion`, `MotionChannelSet`,
  `SourceMetadata`, `MotionClip`, the `MotionStream` intake rules, declarative
  constraints, and the persistent trace format.
- **Out:** retargeting, USD authoring, device and protocol decode
  (`motion-connectors`), avatar-format semantics (the format repositories),
  scheduling (runtimes).

Names are the design policy's
([DESIGN_POLICY.md §42.2](DESIGN_POLICY.md#422-names-are-this-policys-applied-on-arrival));
namespace `openstrata::motion`.

## 2. The joint vocabulary

### 2.1 Version 1

`HumanJoint` version 1 is the **55-joint humanoid** `usd-vrm-plugins` ships
and every producer there already maps onto: the torso and head (`hips`,
`spine`, `chest`, `upperChest`, `neck`, `head`, `leftEye`, `rightEye`, `jaw`),
the legs (`leftUpperLeg`, `leftLowerLeg`, `leftFoot`, `leftToes` and the right
side), the arms (`leftShoulder`, `leftUpperArm`, `leftLowerArm`, `leftHand`
and the right side), and three joints per finger for both hands (thumb
`Metacarpal`/`Proximal`/`Distal`; index, middle, ring and little
`Proximal`/`Intermediate`/`Distal`).

The list and its hierarchy come from the VRM 1.0 humanoid, which is the
reason it is a good vocabulary and not the reason it is this one: the same
joints are what a BVH producer profile, a live pose sender and an MMD role
table map onto. The vocabulary carries **no VRM meaning** — no required-bone
rule, no VRM node binding — and a format repository maps its skeleton onto
it (design policy §5.2).

The design policy's §5.2 lists 22 joints as an example; version 1 is a
superset of it. That was MC-O1, decided on 2026-09-19 before `motionCore`
arrived: every producer already mapped the 55, so a compact version 1 would
only have cut what they deliver. `libs/motionCore` holds it as `HumanJoint`.

### 2.2 Rules

- Values are stable array indices. A joint is appended only before `Count`,
  with a contract version bump; nothing is renumbered.
- **Every joint is optional in a pose.** Whether a target needs a joint is the
  retarget's question ([RETARGETING_POLICY.md §4](RETARGETING_POLICY.md#4-missing-and-extra-joints)).
- One hierarchy table, in one place (`HumanJointParent`), with the nearest
  *present* ancestor for rigs that skip a joint (a rig without `upperChest`
  still parents its shoulders). A second copy of the taxonomy in a consumer is
  a defect: two tables that can disagree produce skeletons that look alike and
  do not compose.
- A joint has a stable lower-camel name and a semantic path
  (`hips/spine/chest/neck/head`), which is plain text; authoring it is
  [USD_MAPPING.md](USD_MAPPING.md)'s.

## 3. Coordinates and units

Canonical motion is **right-handed, +Y up, +Z forward, in metres**, with
rotations as unit quaternions.

The forward axis was recorded, not chosen (design policy §42.5): the avatars
motion is retargeted onto face +Z by their own specification, and the VMC
adapter converts a left-handed +Z-forward sender by flipping X alone. Anything
else would already have shown as a character walking backwards.

Conversion into this basis happens **once, at the boundary** — in a reader, a
connector, or a format repository — and never inside shared code (design
policy §9). A converter that needs it states it as a signed permutation whose
determinant is the handedness:

- a position converts as `M v`;
- a rotation converts as `(w, det(M) · M v)`;
- Euler angles compose into a quaternion by the **right-hand rule always**,
  from the raw numbers, without reference to the source's handedness —
  applying handedness in both places is correct in every axis-aligned test
  and wrong the moment anything turns;
- a named Euler order composes **intrinsically**: `ZXY` is `qZ * qX * qY`.

Conversions are tested *physically* — a direction rotated and compared with
where it must end up — because a component-by-component test agrees with a
mirrored implementation as readily as with a correct one.

## 4. Time

- Timestamps are `double` **seconds**, never frame numbers.
- A sample's timestamp is authoritative. A clip's `nominalFrameRate` is
  descriptive only (design policy §7).
- A stream's timestamps must strictly increase (§9). A clip's samples are
  sorted by timestamp, and every sampler states that as a precondition it
  checks.
- Shared code reads no wall clock. A caller maps a producer's clock onto its
  own with an explicit offset.

## 5. `MotionPose`

### 5.1 Fields

```cpp
struct MotionPose {
    double timestamp = 0.0;                              // seconds (§4)
    RootMotion root;                                     // §5.3
    std::array<GfQuatf, HumanJointCount> localRotations; // local to the semantic parent
    std::bitset<HumanJointCount> validRotations;
    std::optional<std::array<float, HumanJointCount>> confidence;
    std::optional<ContactState> contacts;                // left / right foot: unknown, in, not in
    MotionChannelSet channels;                           // §6
    SourceMetadata metadata;                             // §7
};
```

The design policy's §5.1 also sketches optional per-joint translations. Today
the only body translation any producer delivers is the hips', which §5.3
carries; per-joint translations are added when a producer needs them (MC-O2).

### 5.2 Invariants

- **An absent joint is not an identity rotation.** `validRotations` is the
  truth; a consumer that reads a rotation without its bit reads nothing.
- **A missing sample is held, never faded toward identity** — in a buffer, in
  interpolation, and in live intake — and a channel only one endpoint reported
  is held, not faded toward zero. Fading invents motion no producer described.
- `confidence` is optional. A pose without it is never confidence-gated.
- Rotations are unit quaternions after conversion; a reader that received a
  non-finite or zero quaternion refuses it (design policy §34) rather than
  normalizing it into a plausible pose.
- **Equality.** Exact `==` is defined on every aggregate that crosses a
  boundary (a trace round-trip is defined by it). "Describe the same motion" is
  `NearlyEqual` with one shared `MotionTolerance`, never a hand-picked epsilon.
  Angles between quaternions are computed with the dot product and the length
  product **in the same precision**: mixing them turned `1e-7` into `9e-4` rad
  at zero and cost a red lane on one CPU architecture (measured).

### 5.3 Root motion and the hips

```cpp
struct RootMotion {
    GfVec3f worldPosition;   GfQuatf worldOrientation;
    GfVec3f linearVelocity;  GfVec3f angularVelocity;
    bool hasPosition, hasOrientation, hasLinearVelocity, hasAngularVelocity;
};
```

Root motion is never encoded implicitly in the hips (design policy §5.1.1),
and a `has*` flag distinguishes a missing value from a zero one.

**The record, measured.** A hips translation that is the rig's **only**
translating joint is body translation: it is `RootMotion::worldPosition`,
absolute, in canonical space. The rotation at that joint is the body's
orientation and is `RootMotion::worldOrientation`, *and* it remains the `hips`
local rotation — a rig rooted at its hips has a root path of one joint, and
the duplication is what lets two observations of one session be compared
field for field. Evidence: 207 064 bone-frames over five device sessions in
which every non-root translation equalled its rest offset, and two exports of
one producer agreeing to 4.4e-7 m.

What reaches a target is a **delta** from the rig's own rest hips translation,
decided by the retarget ([RETARGETING_POLICY.md §6](RETARGETING_POLICY.md#6-root-motion)).
A producer with **two** candidate channels (VMC's root position and hips
offset) has no policy yet: which channel is body translation is a fact about
the sender, measured per sender, and until it is measured such a session
retargets in place (MC-O3).

## 6. Channels

`MotionChannelSet` carries every time-varying value that is not a body joint:
expression weights, and later gaze targets and contacts beyond the feet
(design policy §5.3).

- **A channel's name is carried verbatim, and the vocabulary is open.** A VRM
  author's custom expression and a VMC sender's blend-shape name are whatever
  the producer said. Resolving one producer's `Joy` to a rig's `happy` needs
  the rig and belongs to the format repository.
- **Namespaced semantics** keep producers apart: `vrm:happy`, `mmd:あ`,
  `arkit:jawOpen`. A common semantic (`face/blinkLeft`) is promoted only by a
  revision of this contract, never invented by a mapping.
- **Sorted by name, each name once** — an invariant, maintained by the only
  mutator. Two producers that reported the same weights in a different order
  are the same motion, so they must be the same value, and a trace must
  round-trip to the same bytes.
- **An unreported name is not a zero weight**, exactly as an absent joint is
  not an identity rotation.
- **Values are not clamped.** A producer that said 1.5 said 1.5; a clamp
  belongs to whoever applies the weight to a rig.

The design policy sketches a channel as `{TfToken semantic; VtValue value;
float confidence}`. Every channel measured so far is a scalar weight, so a
channel's value is a **`float`** (decided 2026-09-19): `MotionChannel` is a
name and a value, and `motionCore` takes no `vt` dependency for it. Whether
the first non-scalar channel makes the value a `VtValue` or a small closed
variant is what remains of MC-O4. Until then gaze stays a pose field
(`MotionPose::lookAtTarget`, a point), because it is exactly such a channel
and moving it would decide MC-O4 by accident.

## 7. `SourceMetadata`

```cpp
enum class MotionSourceKind { Clip, LiveCapture, Generated, Procedural, Simulated };
struct SourceMetadata {
    MotionSourceKind kind;
    std::string provider, protocol, sourceId;
    std::optional<double> sourceTimestamp;
    std::optional<std::uint64_t> sequenceNumber;
};
```

Provenance is recorded and **never a branch condition** (design policy §4.1).
A consumer that cannot tell a tracker-driven pose from a clip-driven one is
reading the value correctly.

### 7.1 A recorded source's provenance is a neighbour, not a superset

A recorded file carries its own provenance: the format, the file's identity,
the producer label and version, and the profile it was read under.
`usd-vrm-plugins` settled how that relates to `SourceMetadata` before a
converter set its first field
([its MOTION_CONTRACT.md, "Recorded-source provenance"](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/design/MOTION_CONTRACT.md#recorded-source-provenance-v070)),
and `motionSource` arrives with it:

| Recorded-source provenance | `SourceMetadata` |
| --- | --- |
| — | `kind` = `Clip`, always: a file **is** a clip by the time it is read |
| producer | `provider` |
| format | `protocol`: how the values arrived |
| source id | `sourceId` |
| producer version | *dropped* |
| profile id | *dropped* |

The derivation runs one way and deliberately narrows. `SourceMetadata` rides
on every pose and every trace line, and the two dropped fields cannot vary
within a clip. They survive **beside** the motion, in the stage's metadata
([USD_MAPPING.md §5](USD_MAPPING.md#5-metadata)). A test pins the narrowing:
two provenances that differ only in producer version and profile convert to
the same `SourceMetadata`. **A profile id is never a branch condition.**

## 8. `MotionClip` and sampling

A clip is samples, a time range, a descriptive `nominalFrameRate` and
metadata (design policy §7). Sampling answers with a **status as well as a
pose**, because a caller that ignores the status cannot tell a live source
from a stopped one:

| Status | Meaning |
| --- | --- |
| `Sampled` | real bracketing data |
| `Held` | the request fell outside the observed range; the boundary pose was repeated |
| `Extrapolated` | the root carried forward along its last velocity |
| `Unavailable` | nothing to answer with |

Default interpolation is the design policy's §8: linear translation,
shortest-path normalized quaternion interpolation, linear scalar channels,
held discrete values.

**API owed from the OpenExec evidence.** `usd-vrm-plugins`' OpenExec nodes
wrapped these libraries and found where a wrapper could not reach
([its boundary consolidation findings](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/roadmap/boundary-consolidation.md)).
They are fixed when the code arrives, not after:

- `SampleClip(clip, t) -> PoseSampleResult` as a free function, with the
  ordering precondition written on it, so a pure computation need not copy a
  clip into a source object to get the status; one bracket-and-hold search,
  not two.
- A filter's **stateless step**, `Step(prior, pose, options)`, that returns
  the state beside the result — composing one from two streaming calls drops
  the dropout history (measured: 45.0° against 23.8° streamed).
- An N-way blend that can say *there is nothing to blend*, with finite weights
  and one shared instant as preconditions, and its order dependence stated
  (measured: 4.247° between three sources and their reverse).
- Root intake as a free function, `ConditionRootMotion(prior, pose, intake)`,
  instead of a private method of a capture session.

## 9. `MotionStream` intake

A stream receives already-decoded, already-converted poses stamped in the
producer's clock. It owns no transport and no clock, which is what makes a
recorded session replayable byte for byte. Intake decisions are explicit and
**counted**, never silent:

- **Confidence.** Joints below a floor are treated as missing; a pose without
  confidence is never gated.
- **Missing joints.** `HoldLast` or `LeaveUnbound`, never a fade.
- **Root.** `Passthrough`, `Ignore`, or `DeriveVelocity` (fills a missing
  linear velocity from consecutive samples).
- **Ordering.** Timestamps strictly increase. A sample behind the head by more
  than a stale threshold is counted *stale*; a closer one *out-of-order*,
  which is a fault in the producer's clock. Both are refused.
- **Bounds.** A queue has a declared capacity and reports what it dropped
  (design policy §34).

The design policy's §6 leaves pull (`TryRead`) and push (`SetSink`) open; the
imported code is push-into-a-buffer with pull sampling, and a runtime wraps it
in either (MC-O5).

## 10. Recording and the trace format

`MotionRecorder` turns a stream into a clip and carries the per-tick status
counts with it, so a clip baked from a laggy session keeps the evidence of the
lag. It writes no external format (design policy §14).

The persistent trace, `motion-capture-trace`, is line-oriented text with fixed
six-decimal precision, versioned (version 2 added channels), read in every
earlier version, written only in the current one, and **byte-identical on
round-trip** for traces the current writer produced. It stores capture order,
not arrival order; delivery timing is reproduced by a replay schedule. Anything
added to the value types is added to the trace format in the same change, or a
replay stops reproducing the session it recorded.

## 11. Producers, and where each one stops

Every producer terminates at `MotionPose` / `MotionClip`, and nothing
downstream knows which one it was.

| Producer | Stops at | Rule it owns |
| --- | --- | --- |
| a recorded file (BVH) | `MotionClip` | the **path rule**: a mapped joint's local rotation is the composition of the source rotations from just below its nearest mapped ancestor down to itself, root-first — joints in between are on the path, not dropped. The same walk builds the rest pose, from the profile's `rest-offsets`, `stated-rest-rotations` or `first-frame` (a first-frame rest is taken from the first frame entirely). |
| a live pose sender (`motion-connectors`) | `MotionPose` pushed into a stream | decode and conversion |
| a tracker source (`motion-connectors`) | its own observation type, then a solve that produces a sparse `MotionPose` | **a tracker observation is not a pose** and gets no type here; a hips tracker follows §5.3 |
| a generator | `MotionClip` or a pose stream, behind a generator interface | — (design policy §35, "Later") |
| a format repository (`usd-vrm-plugins`, `usd-mmd-plugins`) | `MotionClip` of **body** motion | evaluating its own control rig first when it has one — MMD's IK and append transforms (design policy §42.4) |

### 11.1 A tracker observation gets no type here

A tracker source observes numbered devices, not joints: a position and an
orientation in the receiving application's space, under an index into whatever
the wearer strapped on. `usd-vrm-plugins` decided it gets **no type in
`motionCore`**
([its MOTION_CONTRACT.md, "Tracker observations"](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/design/MOTION_CONTRACT.md#tracker-observations-and-where-they-are-not-v080)).
Every reader of this contract takes a pose: the retargeter, the trace format,
the comparison and the OpenExec nodes. A tracker sample here would have no
reader and three standing obligations: equality, comparison and a place in the
trace format (§10). **`motionCore` begins at the canonical pose.**

The observation type, the region vocabulary, the operator's assignment and the
solve belong together in `motion-connectors`' tracking library, which depends
on this repository and never the reverse. A solve inside an adapter would be a
second motion pipeline. A tracker-driven pose is an ordinary `MotionPose`,
sparse by construction, and its hips follow §5.3, not a second rule.

## 12. Constraints

`MotionConstraintSet` is declarative only: root waypoints, trajectories,
full-body keyframes, joint positions and joint rotations. A pose and a desired
condition are different representations; nothing here solves or evaluates a
constraint.

## 13. Open questions

MC-O1, the joint vocabulary, was decided on 2026-09-19 (§2.1); MC-O4 was
narrowed the same day (§6).

| Id | Question | Resolve by |
| --- | --- | --- |
| MC-O2 | Per-joint translations beyond the hips: which producer needs them, and whether they are optional arrays as design policy §5.1 sketches | a producer that delivers them |
| MC-O3 | Root motion for producers with two translation channels (VMC root position vs hips offset) | one recorded session from each of two VMC senders — operator work in `motion-connectors` |
| MC-O4 | A non-scalar channel's value: `VtValue`, or a closed variant of scalar, vector and point. The scalar case is decided (§6: `float`) | the first non-scalar channel — gaze, when it leaves the pose |
| MC-O5 | `MotionStream`'s public shape: pull, push, or the imported buffer with both wrappers | `motion-connectors`' first consumer |
| MC-O6 | Tracking state: a way to say *tracking lost* that is neither an absent joint nor low confidence | a live producer that can report it |
