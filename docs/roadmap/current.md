# Current

What remains after the imports. Every identity planned for the first
releases is published as a digest-pinned artifact, and every consumer that
held a copy has deleted it. The release history is in the
[changelog](../../CHANGELOG.md) and the [release records](../releases/);
implementation facts are in the [capability matrix](../reference/CAPABILITY_MATRIX.md).
This directory holds only incomplete work.

## What remains

### The first consumer that has never heard of VRM ⬜

- ⬜ **`usd-mmd-plugins`' `mmdMotionAdapter` configures against the installed
  `motionCore`** — the only test of whether the vocabulary is actually
  format-neutral rather than VRM's with the names changed.

The rest of the move is done: `usd-vrm-plugins` consumes every package it
used and holds no copy, `motion-connectors` consumes `motionCore`,
`motionSampling` and `motionRecording` as installed packages, and the parity
evidence named before the move was reproduced against the published packages
with no divergence (in `usd-vrm-plugins`, 2026-09-24).

### Beyond the imports ⬜

Remaining future work includes the generic NPZ payload contract (design policy
§28) and its recorded-source identity decision, IK-assisted retarget, contacts,
blending beyond the imported one, generator interfaces for
`motion-connectors`, and Python bindings. None is started, and none blocks a
consumer.

## Open decisions

They are listed, in the order they block work, in
[the roadmap README](README.md#open-decisions). The next consumer-facing
decision is **USD-O5** (the `Bindings` prim, which `usd-avatar-runtime` needs).
MC-O5 was resolved with `motion-connectors`' first shared connector-to-intake
test; [MOTION §9](../design/MOTION_CONTRACT.md#9-motionstream-intake) records
the public stream shape.

## Completion criteria

This milestone is done when a consumer that is not an avatar format this
ecosystem started from builds against the shared core unchanged.
