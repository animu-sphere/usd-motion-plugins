# USD mapping

> Status: **proposed**, 2026-09-17. **§2–§5 are authored** by `motionUsd`
> since 2026-09-19, except the `Channels` prim, whose names §4.3 decided on
> 2026-09-20 and which nothing authors yet. §6–§7 are not
> implemented here. `usd-vrm-plugins` authors two stages of this family: the
> `.vrma` importer's and the capture recorder's semantic clip. It also bakes
> retargeted animation onto avatars. `motionUsd`'s writer arrived from that
> capture recorder, and the reading half (`motion_retarget`'s `StageIo`)
> arrives with v0.2.0
> ([DESIGN_POLICY.md §42.1](DESIGN_POLICY.md#421-the-core-is-imported-from-usd-vrm-plugins-not-rewritten)).
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

`motionUsd` does not author the prim yet; it still reports the channel names
it did not author (`MotionStageReport::unauthoredChannels`). Authoring them,
and reading them back, arrive with the reading half in v0.2.0. That does not
bump `contractVersion`: a consumer that read a stage without a `Channels` prim
is unaffected by one gaining it, which is the §8 case that adds an optional
prim.

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
  referenced skeleton, so the avatar keeps owning its rig; it authors identity
  `scales` for §4.2's reason. It is a derivative, never written into the
  source motion asset.
- The shape of a `Bindings` prim — typeless with namespaced relationships, or
  a schema — is USD-O5.

## 7. Reading USD back

`UsdSkelAnimation` → `MotionClip` is `motionUsd`'s too: a skeleton whose
joint tokens are semantic paths reads directly; any other skeleton needs a
`RetargetMap` in reverse and is a retarget, not a read. In `usd-vrm-plugins`
the reading lives only in a CLI, where an OpenExec bundle cannot call it, so
the bundle carries a second copy; one library home ends that (its OpenExec
sampling finding). Both copies also drop `RootMotion::worldOrientation`,
which a reader here must carry.

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
