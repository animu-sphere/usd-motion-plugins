# Capability matrix

The only document that says what this repository implements. A capability is
listed as supported only with a test behind it.

Vocabulary: **supported** · **approximated** · **unsupported** · **—**
nothing implemented. The "Implemented elsewhere" column says where the
behaviour exists today, before it moves here; it is not a claim about this
repository.

Status (2026-09-19): **the core value types are implemented** (`motionCore`, imported).

| Capability | Status | Contract | Implemented elsewhere | Release |
| --- | --- | --- | --- | --- |
| `HumanJoint` (vocabulary version 1, 55 joints, one hierarchy), `MotionPose`, `RootMotion`, `SourceMetadata`, `MotionClip`, exact `==` and `NearlyEqual` | supported — `motionCore_unit`, `motionCore_compare` | [MOTION §2–§7](../design/MOTION_CONTRACT.md) | — (imported 2026-09-19) | v0.1.0 |
| `MotionChannelSet` with scalar values | supported — `motionCore_unit`, `motionCore_compare` | [MOTION §6](../design/MOTION_CONTRACT.md#6-channels) | — (imported 2026-09-19) | v0.1.0 |
| Clip sampling with status, interpolation, resample, filter, blend | — | [MOTION §8](../design/MOTION_CONTRACT.md#8-motionclip-and-sampling) | `usd-vrm-plugins` `motionRuntime` | v0.1.0 |
| Stream intake, recorder, `motion-capture-trace` | — | [MOTION §9–§10](../design/MOTION_CONTRACT.md#9-motionstream-intake) | `usd-vrm-plugins` `motionRuntime` | v0.1.0 |
| Authoring a motion stage | — | [USD §2–§5](../design/USD_MAPPING.md#2-the-standalone-motion-stage) | `usd-vrm-plugins` `motion_capture`, `.vrma` importer | v0.1.0 |
| `SkeletonDescriptor`, `RetargetMap`, rest-pose correction, root-motion modes, retarget diagnostics | — | [RETARGET](../design/RETARGETING_POLICY.md) | `usd-vrm-plugins` `vrmRetarget` | v0.2.0 |
| Reading `UsdSkelAnimation` into a clip; baking onto a target skeleton | — | [USD §6–§7](../design/USD_MAPPING.md#6-motion-on-an-avatar) | `usd-vrm-plugins` `motion_retarget` | v0.2.0 |
| BVH through producer profiles | — | design policy §27 | `usd-vrm-plugins` `motionSource`, `motionBvh` | v0.4.0 |
| OpenExec motion nodes | — | design policy §21 | `usd-vrm-plugins` `execMotion` | v0.5.0 |
| Generic NPZ | — | design policy §28 | nowhere | later |
| IK-assisted retarget, foot locking, contacts | — | design policy §12.2 | nowhere | later |
| Device and protocol input | unsupported by design | design policy §3.1 | `motion-connectors` | — |
| VRM, VRMA, PMX, VMD semantics | unsupported by design | design policy §3.2 | the format repositories | — |
