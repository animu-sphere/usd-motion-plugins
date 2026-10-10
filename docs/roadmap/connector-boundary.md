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

The canonical intake and MC-O7 decision are recorded in
[MOTION §9.1](../design/MOTION_CONTRACT.md#91-canonical-intake-and-acquisition-envelopes),
with installed-package evidence in the
[capability matrix](../reference/CAPABILITY_MATRIX.md). Remaining external
bridge acceptance is Phase B below.

## Connector Boundary Phase B — Live bridge consumption

- ⬜ Adopt the owner intake and availability/reset/alignment operations when
  `VmcLiveSource` / `MocopiLiveSource` are separated from acquisition. Keep
  source-specific bridges outside this repository; `usd-avatar-runtime` is
  the preferred composition owner.
- ⬜ Select external restart/discontinuity/missing/stale policy using
  [MOTION §9.1](../design/MOTION_CONTRACT.md#91-canonical-intake-and-acquisition-envelopes).
  The connector observes restart, receive time and source state; the runtime
  maps that evidence to explicit owner operations.
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

Owner API correctness and canonical installed consumption are tracked in the
[capability matrix](../reference/CAPABILITY_MATRIX.md). They do not establish
the external acquisition-envelope actor-routing/restart acceptance gate.

## Connector Boundary Phase C — Generic tracker solve evaluation

- ⬜ Resolve the remaining [MC-O8](../design/MOTION_CONTRACT.md#13-open-questions)
  motion-owned semantic solve input and component contract. Keep observation
  identity, regions, operator assignment and identity applicability upstream.
- ⬜ Specify an API without device/source names, reusable across observation
  providers, with sparse position/orientation and explicit confidence behavior.
  Retain the direct solve's compatibility placement until the contract exists.
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
