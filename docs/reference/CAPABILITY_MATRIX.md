# Capability matrix

The only document that says what this repository implements. A capability is
listed as supported only with a test behind it.

Vocabulary: **supported** · **approximated** · **unsupported** · **—**
nothing implemented. The "Implemented elsewhere" column says where the
behaviour exists today, before it moves here; it is not a claim about this
repository.

Status (2026-09-19): **the core value types, sampling, recording and
authoring a motion stage are implemented** (`motionCore`, `motionSampling`,
`motionRecording`, `motionUsd`, imported).

| Capability | Status | Contract | Implemented elsewhere | Release |
| --- | --- | --- | --- | --- |
| `HumanJoint` (vocabulary version 1, 55 joints, one hierarchy), `MotionPose` with its non-optional `metadata`, `RootMotion`, `SourceMetadata` with a sample's stamp and counter, `MotionClip`, exact `==` and `NearlyEqual` | supported — `motionCore_unit`, `motionCore_compare` | [MOTION §2–§7](../design/MOTION_CONTRACT.md) | — (imported 2026-09-19) | v0.1.0 |
| `MotionChannelSet` with scalar values | supported — `motionCore_unit`, `motionCore_compare` | [MOTION §6](../design/MOTION_CONTRACT.md#6-channels) | — (imported 2026-09-19) | v0.1.0 |
| Clip sampling with status, interpolation, resample, filter, blend | supported — `motionSampling_unit`, `motionRecording_liveCapture`; the §8 findings are fixed: `SampleClip`, `PoseFilter::Step`, an N-way blend that can say *nothing to blend*, `ConditionRootMotion` | [MOTION §8](../design/MOTION_CONTRACT.md#8-motionclip-and-sampling) | — (imported 2026-09-19) | v0.1.0 |
| Stream intake, recorder, `motion-capture-trace` (read versions 1–4, write 4), each sample's stamp and counter kept through intake, sampling, recording and the trace | supported — `motionRecording_liveCapture`, `_corpus` (7 traces, byte-identical round trip), `_traceGen`; the published `MotionStream` shape is MC-O5 | [MOTION §9–§10](../design/MOTION_CONTRACT.md#9-motionstream-intake) | — (imported 2026-09-19) | v0.1.0 |
| Authoring a motion stage: `/Animation/{Skeleton,Body}`, 30 time codes per second, identity `scales`, `motion:timeCodesPerSecond`, `customData.motion` | supported — `motionUsd_unit`, which opens each stage through OpenUSD and checks what UsdSkel resolves; channels and look-at targets are reported, not authored (USD-O4) | [USD §2–§5](../design/USD_MAPPING.md#2-the-standalone-motion-stage) | — (imported 2026-09-19 from `usd-vrm-plugins` `motion_capture`) | v0.1.0 |
| `SkeletonDescriptor`, `RetargetMap`, rest-pose correction, root-motion modes, retarget diagnostics | — | [RETARGET](../design/RETARGETING_POLICY.md) | `usd-vrm-plugins` `vrmRetarget` | v0.2.0 |
| Reading `UsdSkelAnimation` into a clip; baking onto a target skeleton | — | [USD §6–§7](../design/USD_MAPPING.md#6-motion-on-an-avatar) | `usd-vrm-plugins` `motion_retarget` | v0.2.0 |
| BVH through producer profiles: parse and extract, match a declared profile, convert to `MotionClip`, author a motion stage with the producer's rest | supported — `motionSource_*` (8), `motionBvh_*` (10, a recorded export among them), `motion_bvh_inspect_*`, `motion_convert_clip`, `workspace_motion_profiles` and its `_absent` pair | design policy §27; [USD §3](../design/USD_MAPPING.md#3-the-skeleton) | — (imported 2026-09-19, ahead of its release) | v0.4.0 |
| OpenExec motion nodes | — | design policy §21 | `usd-vrm-plugins` `execMotion` | v0.5.0 |
| Generic NPZ | — | design policy §28 | nowhere | later |
| IK-assisted retarget, foot locking, contacts | — | design policy §12.2 | nowhere | later |
| Device and protocol input | unsupported by design | design policy §3.1 | `motion-connectors` | — |
| VRM, VRMA, PMX, VMD semantics | unsupported by design | design policy §3.2 | the format repositories | — |
