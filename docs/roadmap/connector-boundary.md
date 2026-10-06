# Connector boundary consolidation

Incomplete work implementing the accepted
[motion/connector boundary](../design/DESIGN_POLICY.md#44-motion-and-connector-boundary),
2026-10-06. This is **Connector Boundary Phase A-D**, separate from Migration
Phase A-F and Runtime Boundary Phase 1-5. Existing intake, sampling, recording
and retargeting facts remain in the
[capability matrix](../reference/CAPABILITY_MATRIX.md). No release is assigned
yet. This track prepares motion-owned APIs; external bridge migration and
connector implementation remain with their owners.

## Connector Boundary Phase A — Canonical intake

Finite timestamp/adjacent-span enforcement and installed canonical intake
coverage are implemented; see [MOTION §9.1.1](../design/MOTION_CONTRACT.md#911-existing-temporal-primitives)
and the [capability matrix](../reference/CAPABILITY_MATRIX.md). The metadata/input-state
decision below remains open.

- ⬜ Audit the public `LiveCaptureSource` intake and its installed consumption;
  retain `Push(const MotionPose&)` wherever it suffices and document canonical
  values as the only accepted inputs.
- ⬜ Resolve [MC-O7](../design/MOTION_CONTRACT.md#13-open-questions): decide
  whether a pose plus motion-owned timestamp/status/value metadata needs a
  new overload or grouped sample value. Do not accept or reproduce
  `MotionFrame`, `IMotionConnector` or connector session state.
- ⬜ Specify time domain, provenance/sequence, ownership and compatibility;
  keep input state distinct from sampling-result status. Update the motion
  contract and implementation inventory when any API extension lands.

Gate: a consumer builds against installed motion packages using canonical
values only; motion headers have no connector dependency. Existing
ordering, conditioning, sampling and trace/replay tests retain parity.
The owning contract is
[MOTION §9.1](../design/MOTION_CONTRACT.md#91-canonical-intake-and-acquisition-envelopes).

## Connector Boundary Phase B — Live bridge consumption

- ⬜ Prepare intake APIs that external composition can use when
  `VmcLiveSource` / `MocopiLiveSource` are separated from acquisition. Keep
  source-specific bridges outside this repository; `usd-avatar-runtime` is
  the preferred composition owner.
- ⬜ Specify generic restart/discontinuity/missing/stale input handling under
  MC-O7. The connector observes restart, receive time and source state; the
  runtime selects policy; motion APIs apply its generic temporal effects.
- ⬜ Extend reset/alignment/buffering primitives if consumer evidence requires
  more than the existing explicit operations, without source-clock or
  protocol-specific logic.
- ⬜ Support consumer-owned acceptance from an acquisition envelope through
  actor routing to canonical intake, semantic recording and replay. Tests that
  link connector/runtime packages live with those consumers.

Gate: installed-package acceptance demonstrates actor isolation and selected
restart/discontinuity policy, without blending history across an intended
reset or mistaking unavailable input for a zero pose. Motion libraries contain
no connector bridge or raw packet/session capture. Generic motion semantics
and replay tests stay here; acquisition and composition tests stay upstream.

Owner tests now prove reset isolation of held joints, smoothing history and
root-velocity derivation, plus finite alignment/conversion refusal. Installed
consumption covers reset and realignment with a new epoch. This establishes the
existing temporal primitives, not MC-O7's input-state decision or the external
actor-routing/restart acceptance gate.

## Connector Boundary Phase C — Generic tracker solve evaluation

- ⬜ Resolve [MC-O8](../design/MOTION_CONTRACT.md#13-open-questions) by evaluating
  tracker assignment, body solve, pose reconstruction and confidence fusion.
- ⬜ Require an API explainable without device/source names, reusable across
  OpenXR, VRChat OSC and optical mocap, and justified as generic motion
  processing. Retain current connector placement if those conditions fail.
- ⬜ Before any accepted move, define motion-owned input values, component
  placement, dependency edges, validation/comparison/recording obligations and
  parity fixtures. Keep `TrackerObservation` connector-owned and adapt outside
  the motion libraries.

Gate: record the ownership decision and evidence. A positive migration needs
owner algorithm tests and consumer parity before duplicate implementations
are removed. A negative decision closes the evaluation without inventing a
solver here. No evaluation or migration permits a reverse dependency.
The owning contract is
[MOTION §11.1](../design/MOTION_CONTRACT.md#111-a-tracker-observation-gets-no-type-here).

## Connector Boundary Phase D — CI enforcement

- ⬜ Audit existing boundary checks and add missing manifest/library-graph,
  include and direct/transitive link coverage for `motion-connectors`, OpenXR,
  MediaPipe, OSC, WebSocket implementations, device SDKs, browser APIs and
  network sockets.
- ⬜ Guard public APIs against connector-owned types and generic processing
  against source-name branches. Allow source/protocol names as value
  provenance rather than interpreting them as dependency violations.
- ⬜ Keep `motionCore` independent of stage APIs, OpenExec and filtering,
  recording/retarget implementations; retain the documented foundation-type
  allowance until the separate WS-O5 migration.
- ⬜ Verify the guards reject representative forbidden includes/edges and
  allow installed canonical consumers, and run the relevant gates in CI.

Gate: automated checks detect each forbidden dependency class at its
appropriate layer, without mistaking provenance data for an SDK dependency.
The structural contract is
[WORKSPACE §2.6](../architecture/WORKSPACE.md#26-connector-boundary-target).
This audit can proceed alongside the API phases.

## Completion criteria

- Motion libraries depend on no connector type/package, device, protocol,
  browser or network implementation.
- Live intake accepts only motion-owned values and has documented generic
  temporal policy; external adapters route observations without duplicating it.
- Filtering, retargeting and semantic recording have one implementation here;
  raw capture, source-native interpretation and sessions stay with connectors.
- Source-specific live bridges remain external, with installed-consumer parity
  for canonical intake and recording/replay.
- Generic tracker solve ownership is decided under the stated conditions;
  `TrackerObservation` remains connector-owned.
- `motionCore` remains usable without a stage, renderer, network or device,
  and dependency invariants are enforced by automated gates.

Coordinate shared recorder API work with
[Runtime Boundary Phase 4](runtime-boundary.md#runtime-boundary-phase-4--recording)
and public-type stabilization with Runtime Boundary Phase 5. These tracks
reuse the same motion APIs and do not introduce parallel recording or value
contracts.
