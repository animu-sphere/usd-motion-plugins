# USD mapping

> Status: **binding for §2–§5 and §7**, implemented by `motionUsd` since
> 2026-09-20. §6's target-local animation authoring is binding; its scene-side
> `Bindings` prim remains proposed. `usd-vrm-plugins` authors two stages
> of this family:
> the `.vrma` importer's and the capture recorder's semantic clip. It also
> bakes retargeted animation onto avatars. `motionUsd`'s writer arrived from
> that capture recorder and its reader from `motion_retarget`'s `StageIo`
> ([DESIGN_POLICY.md §42.1](DESIGN_POLICY.md#421-the-core-is-imported-from-usd-vrm-plugins-not-rewritten)).
> §7.2's richer motion-domain reader result is accepted direction, not an
> implemented extension to the current reader.
>
> This document owns how motion becomes OpenUSD and back: the standalone
> motion stage, joint tokens, time codes, metadata, and how a motion meets an
> avatar on a stage. On that area it wins over [DESIGN_POLICY.md](DESIGN_POLICY.md)
> §16–§18. Section numbers are stable; open questions are `USD-O<n>`.

---

## 1. Principles

- Standard `UsdSkel` first: `UsdSkelSkeleton`, `UsdSkelAnimation`,
  `UsdSkelBindingAPI` (design policy §4.3). No project schema until a concept
  fails that test.
- `motionUsd` converts; it is not a file-format plugin (design policy §16). A
  format plugin that authors a motion stage calls it.
- A **source motion asset** and a **target-specific derivative** are separate
  assets, related by composition (§6).
- A live runtime never rewrites a stage per frame; USD is authored on bake,
  record or publish.

## 2. The standalone motion stage

```text
/Animation            Scope, the default prim; customData.motion (§5)
  /Skeleton           UsdSkelSkeleton — the canonical semantic skeleton
  /Body               UsdSkelAnimation — body motion, bound to /Animation/Skeleton
  /Channels           non-joint channels (§4.3)
```

Stage metadata: `upAxis = "Y"`, `metersPerUnit = 1`, and `timeCodesPerSecond`
per §4.1.

The prim names are the design policy's: `Skeleton`, `Body`, `Channels`
(USD-O1, decided 2026-09-19). `usd-vrm-plugins`' `.vrma` stage uses
`/Animation/HumanoidSkeleton` and `/Animation/BodyAnimation`, with expressions
under `/Animation/Expressions`. That stage is VRMA's, stays in that
repository and does not change; this stage is a different one, and a
consumer tells them apart by `customData.motion`, not by guessing from names.

## 3. The skeleton

- **Joint tokens are semantic paths** built from the joint vocabulary
  (MOTION_CONTRACT §2): `hips`, `hips/spine`, `hips/spine/chest`, …, over the
  joints present, each parented to its nearest present ancestor. They are
  ASCII by construction.
- Order: the vocabulary's order, restricted to present joints, so a parent
  always precedes its child.
- **Rest transforms are identity except the hips translation** for a clip
  whose rotations are relative to the canonical rest (a capture, a VRMA).
  The hips rest translation is the first observed root position for a
  capture, so root motion arrives downstream as a delta from where the
  session started. A producer whose rest is not identity (a BVH export)
  authors its rest. `motionUsd` takes it as `MotionStageOptions::rest`. The
  producer's rig is then the joint set, every joint holds its rest
  translation, and an unturned joint keeps its rest rotation.
- A target skeleton's **source joint names** — Japanese included — are never
  tokens here: they are preserved beside a deterministic safe identifier, per
  design policy §17.3, by whichever repository authors that skeleton.

## 4. Animation

### 4.1 Time

- A sample at `t` seconds is authored at time code `t × timeCodesPerSecond`.
- **`timeCodesPerSecond` is always 30** (USD-O2, decided 2026-09-19),
  including for a capture sampled at 60 Hz or at an irregular rate. That is
  what `usd-vrm-plugins` authors for every semantic clip and what
  `usd-mmd-plugins` authors for VMD time, so a stage from any of them composes
  with the others without a retime, and the parity evidence that arrives with
  the code needs no conversion. The design policy's "source rate when
  meaningful" is answered by the next rule: the rate is a property of the
  samples, which are kept, and a time code is only where they are written.
- Sample times are authoritative; a time code is their encoding, never their
  meaning.
- **`Body` also states the rate as `motion:timeCodesPerSecond`**, written from
  the same number as the stage metadata. The attribute is a shim: an OpenExec
  computation cannot read stage metadata in 26.08, and the attribute is retired
  when a release lets it
  ([EXEC_CONTRACT.md §5.1](EXEC_CONTRACT.md#51-the-rate-motiontimecodespersecond)).
  It is the only evaluation input a motion asset carries. Every other one is
  the composed scene's (EXEC_CONTRACT §5.6).

### 4.2 Transforms — `scales` is mandatory

`Body` authors `rotations` for every joint and `translations` for the joints
that carry them (the hips), and **always a constant identity `scales` array**.
`UsdSkel` resolves translations, rotations and scales as a unit, and `scales`
has no schema fallback: an animation without it binds correctly and then
resolves **no joint transforms at all**, while every query still succeeds and
returns full-length arrays. Only value comparison catches it (measured, and
the reason every semantic clip in `usd-vrm-plugins` authors the array). Scale
is never animated.

Root motion rides in the hips translation of `Body`, and the stage says which
producer channel it came from in metadata (§5). A separate root prim is not
authored; whether a stage should carry `RootMotion`'s orientation and
velocities explicitly is USD-O3.

### 4.3 Channels

`MotionChannelSet` entries become one prim per channel under `Channels`,
carrying the namespaced semantic verbatim and a time-sampled value.

**USD-O4 is decided (2026-09-20).** One prim per channel, named from the
channel's semantic sanitized into a valid prim name, and two attributes on it:

| Attribute | Type | Content |
| --- | --- | --- |
| `motion:channelName` | `uniform string` | the namespaced semantic, **verbatim** — `vrm:happy`, not `vrm_happy` |
| `motion:channelValue` | `float`, time-sampled | the channel's value (MC-O4: scalar only) |

```usda
def Scope "Channels"
{
    def "vrm_happy"
    {
        uniform string motion:channelName = "vrm:happy"
        float motion:channelValue.timeSamples = {
            0: 0.0,
            30: 1.0,
        }
    }
}
```

The prim is **typeless**, as `Bindings` is proposed to be (USD-O5): a channel
is a name and a number, which namespaced properties on a typeless prim express
without a schema (design policy §4.3).

The name attribute, **not the prim path**, is the key. That is
`usd-vrm-plugins`' measured rule for the same data — it authors VRMA
expressions as `/Animation/Expressions/<name>` with `vrm:expressionName`,
`vrm:expressionType` and a time-sampled `vrm:expressionWeight` — and the
reason is that a sanitized path can differ from the name: `vrm:happy` and
`vrm.happy` are two semantics that sanitize to one prim name. So the rule is
two-sided, and both halves are required:

- **A writer makes the prim names unique**, and a channel set it cannot author
  under distinct names is an error rather than a stage with one channel
  silently overwriting another.
- **A reader keys on `motion:channelName`** and never on the prim's path.

What does **not** come across is `vrm:expressionType`: it classifies a VRM
expression, and a format's classification of its own channel belongs in that
format's namespace, not in the generic mapping.

`motionUsd` authors and reads the prim since 2026-09-20, with the reading
half. That did not bump `contractVersion`: a consumer that read a stage
without a `Channels` prim is unaffected by one gaining it, which is the §8
case that adds an optional prim.

**A channel is read back only where the stage keyed it.** USD holds the last
key forward, so asking a channel's value at every body key would give every
later sample a value the producer never reported, and "an unreported name is
not a value" (MOTION_CONTRACT.md §6) would not survive one trip through a
stage. A channel stated once, without time samples, applies to every sample:
that is what stating it once means.

### 4.4 Gaze points

Implemented locally, unreleased, 2026-10-07 (issue #37): the bound body
animation may carry a custom `point3f motion:lookAtTarget`. It is a target
**point**, in the same canonical clip space as `MotionPose::root.worldPosition`:
right-handed, Y-up, forward +Z, metres. It is not head-local, a direction, or
an already placed runtime-world point. A nonidentity skeleton/scene placement
does not change the values returned by `ReadMotionStage`.

`AuthorMotionStage` keys the attribute only where a sample reports a target;
an origin target is present, and a missing target authors no key. With no
targets the attribute is absent. Present nonfinite points are refused before
any authoring. `MotionStageReport::unauthoredLookAtTargets` remains for source
compatibility and is zero on successful writes. This optional property does
not bump the mapping's contract version (§8).

Reading uses the union of body, channel and gaze key times. As with channels,
a keyed point appears only at its exact authored keys; USD interpolation or
holding must not turn an unreported point into an observation. An authored
default with no keys applies at every resulting pose. If both a default and
keys exist, numeric-time input uses only the keys. Declared-only, missing and
blocked values remain absent; none becomes an origin point. A stage with only
defaults yields one pose at its start time code. Sample seconds are the source
time code divided by its stage rate, never forced to the writer's rate of 30.
Writing the resulting clip encodes those observed poses as keys; the clip does
not retain whether a constant originally came from a USD default.

Authored gaze is refused unless it is `point3f` on a Y-up metre stage with a
finite positive rate. The reader does not infer a forward basis or convert a
noncanonical input: the format owner must supply canonical clip coordinates.
Finite scalar weights are carried without clamping; nonfinite scalar or point
values are refused rather than reported as an absent field.

The host selects the source clip placement and time explicitly and transforms
the point with that placement into runtime-world space **once**, alongside the
body's placement. In a Y-up metre USD scene whose skeleton xform expresses that
placement, `UsdGeomXformCache(timeCode).GetLocalToWorldTransform(skeletonPrim)
.Transform(GfVec3d(target))` provides that scene-space point. A host using a
different runtime-world basis/units converts that scene result explicitly. It
must not apply the animated hips transform again: the point and root position
already share clip space. Retarget body scaling does not imply scaling gaze;
the host must choose that policy. The source LookAt offset and the avatar's
LookAt configuration remain with the format owner.

### 4.5 Explicit format-owner input projection

The overloads of `ReadMotionStage` and `OpenMotionStage` accepting
`MotionStageReadOptions` are the callable handoff. `ReadCanonicalMotionStage`
accepts the same options when the strict authored-rest/identity-placement
source profile is required. The format owner selects
each scalar's absolute name/value attribute paths and a semantic prefix,
and optionally selects a gaze attribute path. The generic reader handles
the common time union and absence rules; it discovers no format-specific
prim or attribute and links no avatar schemas or evaluators. No source stage
is rewritten. A selected gaze path explicitly replaces the common animation
gaze input, avoiding implicit precedence between two sources.

For the existing `usd-vrm-plugins` native VRMA layout, an owner adapter
enumerates **only the selected clip's** expression prims and supplies:

```cpp
MotionStageReadOptions inputs;
// Repeat for each expression prim selected by the VRMA owner.
inputs.channels.push_back({
    "/Animation/Expressions/<selected>.vrm:expressionName",
    "/Animation/Expressions/<selected>.vrm:expressionWeight", "vrm:"});
inputs.lookAtTargetAttributePath = "/Animation/LookAt.vrm:lookAtTarget";
ReadMotionStage(stage, selectedSkeletonPath, inputs, &read, &error);
```

`<selected>` means an actual owner-selected prim name, not an expression
identity. Identity comes from the authored constant string/token name,
verbatim, with the explicitly chosen prefix. For example `custom.face` becomes
`vrm:custom.face`, regardless of the sanitized prim path. Explicit zero,
unclamped finite weights and keyed/default/absent inputs keep their meanings.
Missing name for an authored value, animated/empty names, wrong value types,
invalid paths and duplicate projected identities (including collision with a
common channel) are errors. Avatar aliases, channel arbitration, preset/custom
classification and source LookAt offsets are not projected by this API.

**Supported intake is explicit.** Without these options, the common reader
reads only common scalar/gaze properties and body data. An absent common
field says nothing about native VRMA expression/gaze availability. Missing or
unreported **selected** native fields are absent input; unselected native
fields are unavailable to that read, not evidence that the file omits them.
The owner adapter must expose its selected/supported fields to the host.
Automated VRMA discovery and runtime adapter adoption remain in their owners;
this extension supplies the tested handoff, not their deployment. Splines and
noncanonical point encodings require an owner conversion. `PoseFromStageSample`
and existing OpenExec body-sampling nodes still accept only joint arrays;
hosts using them must route supplementary inputs separately.

Constructed-stage coverage is `motionUsd_inputs`; the installed-consumer lane
also exercises the common round trip and the explicit VRMA-style handoff.
These tests use no native/private asset and claim no native importer or runtime
conformance from a synthetic probe.

## 5. Metadata

`/Animation.customData.motion` (a dictionary; USD expands colon-separated keys
into one):

| Key | Content |
| --- | --- |
| `contractVersion` | the version of this mapping (§8) |
| `jointVocabularyVersion` | `HumanJoint`'s version (MOTION_CONTRACT §2) |
| `sourceFormat` | `vrma`, `bvh`, `vmd`, `capture`, … |
| `sourceProvider` | from `SourceMetadata`, when known |
| `rootMotionSource` | which producer channel became the hips translation |
| `duration`, `sampleCount`, `nominalFrameRate` | descriptive |

Format-specific provenance stays in its own namespace beside it
(`customData.vrma`, `customData.mmd`). A recorded source's provenance is
`customData.source`, a dictionary of strings: the profile id, the producer and
its version, and what the conversion composed or dropped. These are the fields
`SourceMetadata` narrows away (MOTION_CONTRACT.md §7.1). `motion_convert`
authors it, and nothing reads it to decide anything. Runtime-only state is
never authored.

## 6. Motion on an avatar

```text
/World
  /Character            references the avatar asset
  /Motions/Walk         references the motion asset (/Animation)
  /Bindings/CharacterWalk
      target = </World/Character>, source = </World/Motions/Walk>, retarget policy
```

- Source and avatar compose **by reference**, never by sublayering one over
  the other (design policy §18).
- A **baked** retarget is a `UsdSkelAnimation` in the target skeleton's joint
  order, bound with `skel:animationSource` on an **override** of the
  referenced skeleton, so the avatar keeps owning its rig; it authors constant
  **rest scales** for §4.2's reason. A target's scaled rest must be preserved:
  an animated joint takes its whole transform from the animation, so identity
  would replace that scale. It is a derivative, never written into the
  source motion asset.
- The shape of a `Bindings` prim — typeless with namespaced relationships, or
  a schema — is USD-O5.

### 6.1 Authoring evaluated target-local samples

`AuthorSkeletonAnimation` takes a stage the caller has already composed,
absolute skeleton and new animation paths, joint tokens matching the
skeleton's exact order, and `SkeletonAnimationSample` values. Each sample
carries a timestamp in seconds and full local rotation/translation arrays.
These are already evaluated values: the caller retargets and converts them
into the target stage's units and basis first. This boundary adds no edge
from `motionUsd` to `motionRetarget`.

The writer reads the skeleton's rest scales and authors them as one constant
array. It requires the §4.1 rate of 30 and encodes the sample timestamps with
the same frame snap as the standalone writer. It preserves stage metadata,
including units, basis and playback interval; the caller owns those in a
composed scene. The evaluation shim on a standalone `Body` (§4.1) is not added
to this target-specific animation.

All writes go to the derivative stage's root layer. Referenced avatar and
motion layers, the rig's joints/rest/bind attributes and the current edit
target remain unchanged. Empty samples, non-finite values, non-unit
quaternions, time codes that do not increase, mismatched joint arrays,
malformed rest transforms, existing animation paths and authoring through
instances/prototypes are refused. An authoring failure, including a stronger
binding opinion that prevents the new animation from being used, restores
the root layer's content. The writer does not save a file or replace a prior
animation; a caller chooses a new derivative or a new animation path.

This is the baked-animation step of §6. The writer does not construct a
`Bindings` prim, discover an avatar's semantic roles, or resolve retarget policy.

## 7. Reading USD back

`UsdSkelAnimation` → `MotionClip` is `motionUsd`'s too, and it arrived on
2026-09-20 from `usd-vrm-plugins`' `motion_retarget`. A skeleton whose joint
tokens are semantic paths reads directly; any other skeleton needs a
`RetargetMap` in reverse and is a retarget, not a read, and is refused rather
than guessed at.

- `ReadMotionStage` and `OpenMotionStage` answer a `MotionClip`, the
  skeleton's joint tokens and rest transforms, §5's metadata and a list of
  warnings. A warning is never a refusal.
- **The skeleton comes back as values**, not as a `SkeletonDescriptor`: the
  joint tokens and rest transforms are what `BuildSkeletonDescriptor` takes,
  and the descriptor it answers is what `BuildSourceRestPose` takes after it
  ([RETARGETING_POLICY.md §10](RETARGETING_POLICY.md)). So reading a stage
  does not link a retargeter and `motionUsd` keeps its current library edge
  ([WORKSPACE.md §2.1](../architecture/WORKSPACE.md#21-inside-the-repository)).
  This is the current API; §7.2 defines the accepted direction that removes
  the consumer's descriptor/rest assembly step.
- **The producer's rate is not the stage's.** `timeCodesPerSecond` is where
  the samples were written and is always 30 (§4.1); the rate they were taken
  at is `customData.motion.nominalFrameRate`, and a read puts it back on
  `MotionClip::nominalFrameRate`. A reader that took the encoding for the
  measurement would report every 60 Hz capture as 30 Hz.
- **`PoseFromStageSample` is the rule, taking values.** In `usd-vrm-plugins`
  the reading lived only in a CLI, where an OpenExec bundle cannot call it, so
  the bundle carried a second copy; this is the library home that ends it (its
  OpenExec sampling finding). `execMotion` calls it since 2026-09-20 and holds
  no copy. The switch changed what that bundle answers, which is the point and
  not a side effect: see §7.1.
- **`RootMotion::worldOrientation` is carried.** Both copies dropped it. The
  hips rotation is the body's orientation *and* stays the local rotation
  ([MOTION_CONTRACT.md §5.3](MOTION_CONTRACT.md#53-root-motion-and-the-hips)),
  so a reader that kept only the local rotation lost the body's facing.
- **The rate is the stage's.** `Body`'s `motion:timeCodesPerSecond` is a shim
  for a computation that cannot read stage metadata (EXEC_CONTRACT.md §5.1),
  and a reader that quietly preferred one of the two would make a stage on
  which they disagree sample at two rates without saying so. The stage's is
  used and the disagreement is a warning.
- **A stage that claims none of this mapping still reads.** `usd-vrm-plugins`'
  `.vrma` stage is standard `UsdSkel` over the same joint tokens and carries
  no `customData.motion`; an absent `contractVersion` is a fact about the
  stage, not a defect in it.

### 7.1 What the shared rule changed for `execMotion`

The copy the bundle carried left `root.hasOrientation` false, reasoning that a
`UsdSkelAnimation` states rotations per joint and no separate root
orientation. [MOTION_CONTRACT.md §5.3](MOTION_CONTRACT.md#53-root-motion-and-the-hips)
overrules it: the hips rotation **is** the body's orientation, and the
duplication is the record rather than an encoding accident. So a clip-sourced
pose now carries one, and every node that reads `RootMotion` sees it.

Which nodes that is, from the libraries rather than from inspection:

- **`motion.filterPose` smooths the root orientation by default.**
  `PoseFilter::Options::filterRootOrientation` defaults to true, and the field
  it names was previously absent from every clip-sourced pose, so the flag was
  inert whatever a clip authored. A clip that authors no filter policy now has
  its root orientation slerped, on the same step as every other rotation.
  `execMotion_pose` pins both the default and the explicit refusal.
- **`motion.interpolatePose` interpolates it.** `SampleClip` between two
  bracketing samples slerps the root orientation when both carry one, and
  before the switch neither ever did.
- **`motion.extractRootMotion` carries an orientation it used to drop.**
  `ConditionRootMotion` does not branch on the field, so the rule is unchanged
  and only the value it returns is fuller.
- **`motion.rootTransform` rotates the placement** for a clip that turns its
  hips, where it used to translate only — which is what §6's bake already
  does. The fixtures do not turn theirs, so no golden moved.
- **`motion.blendPoses` is unaffected**: `BlendPoses` does not read `root` at
  all.

**What a read cannot recover**, because the stage does not carry it. A
`UsdSkelAnimation` states a rotation for every joint at every key: §4.2's
writer authors an unobserved joint at its rest, and nothing distinguishes
that from an observed one. So a read answers `validRotations` set for every
joint the skeleton carries, and `MissingJointPolicy` is not inferable from a
stage. The same rule makes `RootMotion::hasOrientation` true wherever the
hips turn, whether or not the producer stated a root orientation — which is
§5.3's duplication read in the only direction a stage allows. A clip that
must keep which joints were observed keeps its trace
([MOTION_CONTRACT.md §10](MOTION_CONTRACT.md#10-recording-and-the-trace-format)),
not its stage.

### 7.2 Motion-domain reader results

**Accepted direction, 2026-10-06; API shape proposed.** `motionUsd` owns generic
USD interpretation through motion-domain values. Runtime consumers should
receive a coherent clip, skeleton, source rest and metadata, rather than
reconstructing them from raw USD arrays in their own `StageClip` wrapper.
The existing §7 reader remains available until an extension lands with tests.

Illustrative result shapes, not installed API declarations:

```cpp
struct MotionSkeletonRead {
    SkeletonDescriptor skeleton;
    SourceRestPose rest;
    MotionSkeletonMetadata metadata;
};

// Extend the existing MotionStageRead, preserving compatible fields:
// clip, skeleton, metadata, warnings, plus an owner-built sourceRest.
```

A skeleton reader accepts an explicitly selected `UsdSkelSkeleton`; a stage
reader accepts the selected source path and returns the clip and its rest
together. Exact signatures, failure/report types and compatibility with the
existing `MotionStageRead::skeleton` arrays are settled during
[Runtime Boundary Phases 2-3](../roadmap/runtime-boundary.md). Candidate headers
are `RestPoseReader.h` and `Validation.h`, alongside the existing
`MotionStage.h`, `ClipReader.h` and `ClipWriter.h`. The scoped
`SkeletonReader.h` surface below is implemented; the coherent typed result
illustrated above remains proposed.

The reader owns joints/rest extraction, topology checks, decomposition via
owner value algorithms, motion-domain descriptor/rest construction, generic
joint mapping helpers and stage metadata. An arbitrary skeleton can become a
generic descriptor without being a semantic motion clip; §7's clip reader must
still refuse to infer humanoid semantics from non-semantic joint names.
Format-provided maps and required-bone sets stay explicit caller inputs.

Generic motion import/export policy must state how `metersPerUnit`, axes,
time codes and skeleton placement are converted or rejected. Time-code and
placement validation belongs here, including checks now carried by runtime
wrappers. This does not move scene scheduling, avatar placement/state or
renderer publication into `motionUsd`, or change §6.1's target-unit input
contract without a separate contract change.

WS-O4 selects a public `motionUsd` → `motionRetarget` dependency for this
extension, as accepted on 2026-10-07 in
[WORKSPACE.md §2.5](../architecture/WORKSPACE.md#25-runtime-boundary-target).
The descriptor/rest types, builders and validators stay with their existing
owner and remain reusable without a USD stage. Dependency wiring and the
typed reader API are still unimplemented; callers should not have to reproduce
builders to avoid linking a reader.

### 7.2.1 Scoped strict array readers

Implemented locally, unreleased, 2026-10-06: `SkeletonReader.h` adds
`ReadSkeleton(stage, path, read, diagnostic)` and
`ReadCanonicalMotionStage(stage, path, read, diagnostic)`. This slice keeps
the existing dependency graph and raw `MotionStageSkeleton` representation;
it does not implement the WS-O4 edge or the typed descriptor/source-rest result.
Consumers invoke existing motionRetarget builders rather than duplicate them.

`ReadSkeleton` returns owned default-time parent-local rest matrices in metres,
joint tokens and parent indices, authored-rest presence, source unit metadata,
and separate rigid skeleton world translation/rotation. It requires an explicit
absolute skeleton path, Y-up and a caller-asserted canonical forward basis.
Positive nonuniform rest scale and multiple/auxiliary roots are preserved.
Nonpositive/nonfinite units, malformed rest/token/topology, immediate-parent mapping
disagreement, nonfinite or float-unrepresentable rest, reflection, zero scale,
shear beyond `1e-6`, affine deviations beyond `1e-12` and nonrigid placement are
refused. Accepted affine roundoff is canonicalized only in the owned copy.

`ReadCanonicalMotionStage` additionally requires metre units, identity world
placement and a finite positive encoding rate because semantic sample
translations use canonical metres. It preserves existing clip/metadata/warnings
and replaces the raw skeleton arrays with validated authored rest. The original
`ReadMotionStage` remains permissive, including missing-rest fallback/warnings.

Both APIs leave the caller's result untouched on refusal and return a
`SkeletonReadDiagnostic` containing unmodified `MOTION_USD_*` code, subject and
detail. Neither retains a stage/prim handle or authors a layer. Correctness and
graph coverage are in `motionUsd_skeletonReader` and `motionUsd_boundaries`;
runtime installed-consumer tests check marshaling and diagnostic/lifetime parity.

### 7.3 Reader time-code policy

**Binding numeric validation, unreleased, 2026-10-07.** The common stage reader
validates the complete body/channel/gaze key union before resolving values.
Every time code must be finite and divide by the encoding rate into finite,
strictly increasing double seconds. Distinct keys that collapse to the same
second, conversion overflow and nonfinite adjacent timestamp differences are
refused; silently merging them would lose input timing. Negative time codes
are valid. Authored keys are not cropped by the stage's playback interval.
Only a stage without keys uses `startTimeCode` for its single default pose,
and that start must also convert to finite seconds.

`ReadMotionStage` retains its rate fallback: nonfinite or non-positive encoding
rates use 30 with a warning. `ReadCanonicalMotionStage` refuses those rates as
`MOTION_USD_RATE` before invoking the common reader. Common-reader temporal
refusals are forwarded as `MOTION_USD_READ`, with the selected skeleton subject
and reader detail; the strict destination stays untouched. Producer
`nominalFrameRate` must be finite and positive to become the clip's rate;
otherwise the effective encoding rate is used with a warning. Original
producer metadata remains available verbatim, including an unusable rate.

The stage-free `PoseFromStageSample` uses the same finite rate/conversion rule
for one pose. At default time it ignores the unused numeric time code and
returns timestamp zero; it still requires a finite positive encoding rate.
It cannot validate neighbouring timestamps because it receives only one sample.

`motionUsd_timeCodes` covers numeric extremes, defaults, negative keys, playback
bounds, rate fallback and supplementary key timing. `workspace_installed_consumer`
checks selected-gaze overflow, strict output preservation and the shared value
rule through installed packages. These guards do not change authored mapping
version 1 or add a dependency edge. Typed clip/rest assembly remains §7.2 work.

## 8. Versioning

The mapping carries `contractVersion`, starting at 1 with the first release
that authors a stage. A change that alters how an existing consumer interprets
a stage bumps it; adding an optional prim or key does not.

## 9. Open questions

USD-O1 (prim names, §2) and USD-O2 (time codes, §4.1) were decided on
2026-09-19, and USD-O4 (channel attribute names, §4.3) on 2026-09-20.

| Id | Question | Resolve by |
| --- | --- | --- |
| USD-O3 | Whether root orientation and velocities are authored explicitly, or only the hips translation | a consumer that reads them back |
| USD-O5 | The `Bindings` prim: typeless with namespaced properties, or a schema that passes design policy §4.3 | `usd-avatar-runtime`'s first composed scene |
| USD-O6 | Whether `MOT-O2` in `usd-mmd-plugins` — a directly opened `.vmd` — can use this stage at all, since a VMD without a model has control-rig tracks, not body motion | that repository, with this mapping |
