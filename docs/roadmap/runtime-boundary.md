# Runtime boundary consolidation

Incomplete work implementing the accepted
[motion/runtime boundary](../design/DESIGN_POLICY.md#43-motion-and-avatar-runtime-boundary),
2026-10-06. This is **Runtime Boundary Phase 1-5**, separate from the historical
Migration Phase A-F imports. Existing sampling, retargeting, recording and USD
APIs are recorded in the [capability matrix](../reference/CAPABILITY_MATRIX.md);
the tasks below extend or consolidate those APIs. They do not claim completed
runtime migration. No release is assigned yet.

## Runtime Boundary Phase 1 — Validation 🚧

- ⬜ Integrate owner reports into the runtime's structured diagnostic channel
  without redefining codes, and remove duplicate generic validation after
  parity is proven. Runtime state/ABI validation stays with the runtime.

Gate: malformed-input tests and diagnostic forwarding parity pass through
installed packages; consumers no longer maintain those generic checks.
The installed owner report surface and mutable-value validation boundary are
defined in [MOTION §14](../design/MOTION_CONTRACT.md#14-generic-validation-ownership);
owner tests and installed-package coverage are in the
[capability matrix](../reference/CAPABILITY_MATRIX.md). Consumer forwarding
and duplicate-removal evidence remain required for this phase's gate.

## Runtime Boundary Phase 2 — USD skeleton and rest readers

A scoped strict-array reader is implemented without changing the component
graph: `motionUsd::ReadSkeleton` owns default-time rest/token/topology/matrix
validation, metre conversion and separate rigid placement values. Runtime
`SkeletonBinding` now invokes it and the existing owner descriptor builder.
`motionUsd_skeletonReader` and runtime installed consumers cover the slice.
The coherent typed descriptor/source-rest result below remains gated by WS-O4;
this evidence does not close Phase 2.

- ⬜ Resolve [WS-O4](../architecture/WORKSPACE.md#6-open-questions) before
  implementation and update dependency declarations and gates.
- ⬜ Add an explicitly selected USD skeleton reader returning descriptor,
  source rest and metadata; reuse value builders for joints, decomposition,
  topology, source/target rest and generic mapping.
- ⬜ Define generic stage metadata, placement, units and axis policies,
  including explicit unsupported-input rejection.
- ⬜ Replace the generic conversion part of runtime `SkeletonBinding` with
  owner calls, retaining avatar layout/state and format binding adaptation.

Gate: resolved UsdSkel tests cover non-trivial hierarchies, authored/missing or
malformed rests, source-rest extraction, units, axes, placement and topology;
the runtime contains no generic USD-to-motion skeleton conversion.
The owning contract is [USD §7.2](../design/USD_MAPPING.md#72-motion-domain-reader-results).

## Runtime Boundary Phase 3 — StageClip absorption

The scoped `ReadCanonicalMotionStage` now owns source skeleton and
unit/axis/rate/placement checks; runtime `StageClip` delegates to it and invokes
owner descriptor/source-rest builders, without creating a source avatar
binding. The original clip reader remains compatible. Full typed clip/rest
consolidation and wrapper absorption below remain open.

Shared time-code numeric validation is implemented: complete input keys must
convert to finite increasing seconds with finite adjacent spans, negative keys
are accepted, and playback bounds do not crop authored samples. Rate fallback
and strict refusals are defined in [USD §7.3](../design/USD_MAPPING.md#73-reader-time-code-policy),
with `motionUsd_timeCodes` and installed-package coverage. This closes the
numeric time-code policy slice, not the typed clip/rest or runtime acceptance gate.

- ⬜ Extend `MotionStageRead` compatibly to return clip and owner-built source
  rest together, with skeleton and metadata.
- ⬜ Complete source skeleton selection, extraction and placement checks for
  the coherent typed clip/rest result in `motionUsd`.
- ⬜ Replace runtime `adapters/motion-usd/StageClip` assembly with an owner
  result; any retained runtime wrapper only maps configuration and diagnostics.
- ⬜ Exercise `motionUsd` → clip/rest → `SampleClip` → `Retarget` → runtime
  adapter → `EvaluatedAvatarState` in the runtime integration suite.

Gate: clip/rest round trips and time-code policy tests pass here, and installed
consumer acceptance proves the runtime adapter marshals, invokes and publishes
without rebuilding a motion-domain convenience object.

## Runtime Boundary Phase 4 — Recording

- ⬜ Establish the adapter-facing generic recorder/replay surface using the
  existing `MotionRecorder`, intake, trace and replay APIs where sufficient.
- ⬜ Centralize recording format, resampling, compression and timestamp policy
  in motion APIs as needed by consumers; do not imply compression exists today.
- ⬜ Replace any runtime recording algorithms with owner calls; the runtime
  selects sessions/avatars and publishes recorded results.

Gate: recording/replay parity and runtime publication integration pass;
recording semantics have one owner. The contract is
[MOTION §10.1](../design/MOTION_CONTRACT.md#101-recording-versus-runtime-publication).

## Runtime Boundary Phase 5 — API and ABI stabilization

- ⬜ Validate the same installed APIs with a second consumer, such as a
  VRM/MMD format adapter or external runtime; document the necessary thin ABI
  mappings and shared consumption by the format repositories.
- ⬜ Audit for duplicated canonical values/joint vocabulary and generic
  sampling, retargeting, validation, rest extraction and recording code.
- ⬜ Resolve [WS-O5](../architecture/WORKSPACE.md#6-open-questions), implement
  the public-type migration and verify OpenUSD dependency isolation.
- ⬜ Select API/ABI freeze candidates from consumer evidence; record version
  and compatibility decisions before a freeze.

Gate: generic motion values/algorithms contain no format, runtime or renderer
types; consumers map to their own ABI and forward owner diagnostics. Full
OpenUSD isolation must be demonstrated, not inferred from the absence of stage
APIs in `motionCore`.

## Completion criteria

- The runtime has no sampling or retarget algorithm or generic motion validation.
- `motionUsd` owns USD-to-motion skeleton conversion and source-rest extraction.
- Motion APIs own recording semantics, with recording/replay parity here.
- Runtime adapters marshal inputs, invoke owner APIs and publish state.
- VRM/MMD can use the same canonical motion APIs; a second consumer provides
  API/ABI stabilization evidence.
- Owner correctness tests pass here; runtime integration proves the full path
  to `EvaluatedAvatarState` and diagnostic forwarding.

Generic skeleton/stage reading can proceed independently of USD-O5's scene-side
`Bindings` prim. The latter remains a separate composition decision in
[current.md](current.md).
