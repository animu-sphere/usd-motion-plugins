# Current

What remains after the initial imports. Those package moves do not establish
that all later runtime wrappers and duplicated generic logic have been
consolidated. The release history is in the
[changelog](../../CHANGELOG.md) and the [release records](../releases/);
implementation facts are in the [capability matrix](../reference/CAPABILITY_MATRIX.md).
This directory holds only incomplete work.

## What remains

### Motion/runtime boundary consolidation ⬜

The accepted [ownership policy](../design/DESIGN_POLICY.md#43-motion-and-avatar-runtime-boundary)
places generic motion semantics, mathematics and interchange here, with avatar
orchestration and publication in `usd-avatar-runtime`. The ordered work and
acceptance gates are in [runtime-boundary.md](runtime-boundary.md): validation,
USD skeleton/rest APIs, StageClip absorption, recording, then API/ABI
stabilization. Reuse implemented motion APIs and remove consumer duplication
after parity evidence. This track does not depend on the `Bindings` decision.

### Motion/connector boundary consolidation 🚧

The accepted [ownership policy](../design/DESIGN_POLICY.md#44-motion-and-connector-boundary)
keeps acquisition envelopes and source/device/protocol interpretation in
`motion-connectors`, canonical processing/semantic recording here, and their
bridge in external composition, preferably `usd-avatar-runtime`.
[connector-boundary.md](connector-boundary.md) orders Connector Boundary
Phase B-C: adopt motion-owned intake in external live bridge
consumption and evaluate source-independent tracker solve. Dependency gates
are recorded in [WORKSPACE §2.4](../architecture/WORKSPACE.md#24-enforcement).
`MotionFrame` and `TrackerObservation` remain
connector-owned. No generic solver migration is claimed.

### A composed motion binding ⬜

- ⬜ **USD-O5: the scene-side `Bindings` prim** — decide and implement the
  properties that relate a composed source motion asset, target avatar and
  retarget policy, with `usd-avatar-runtime`'s first scene consumer.
- ⬜ **EX-O3: scene-side evaluation attributes** — settle their placement
  before or with that binding, as scheduled in the roadmap index.

### Beyond the imports ⬜

Remaining future work includes the generic NPZ payload contract (design policy
§28) and its recorded-source identity decision, IK-assisted retarget, contacts,
blending beyond the imported one, generator interfaces for
`motion-connectors`, and Python bindings. None is started, and none blocks a
consumer.

## Open decisions

They are listed, in the order they block work, in
[the roadmap README](README.md#open-decisions). Runtime Boundary Phases 2-3 now
need runtime adoption of the implemented typed skeleton and clip/rest reader API
([USD §7.2](../design/USD_MAPPING.md#72-motion-domain-reader-results));
the next composition decision remains **USD-O5** (the `Bindings` prim).
Connector Boundary Phase B needs external adoption of the intake availability
and reset/alignment contract ([MOTION §9.1](../design/MOTION_CONTRACT.md#91-canonical-intake-and-acquisition-envelopes));
Phase C decides **MC-O8** (conditional generic tracker solve ownership).
MC-O5 was resolved with `motion-connectors`' first shared connector-to-intake
test; [MOTION §9](../design/MOTION_CONTRACT.md#9-motionstream-intake) records
the public stream shape.

## Completion criteria

Runtime boundary completion is defined in
[runtime-boundary.md](runtime-boundary.md#completion-criteria).
Connector boundary completion is defined in
[connector-boundary.md](connector-boundary.md#completion-criteria).

The next composition milestone is done when a consumer can state and read
the source/target/policy relationship without redefining the generic motion
contract or modifying either referenced asset.
