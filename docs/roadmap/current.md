# Current — after v0.5.0

Status: **v0.5.0 is published** (2026-09-20,
[its record](../releases/v0.5.0.md)). Every identity planned for that release
is published as a digest-pinned artifact. What remains is not more imports: it
is the other side of the move, in the repositories that consume these packages,
and the evidence that the move preserved what it moved.

The scaffold and release history are in the
[changelog](../../CHANGELOG.md) and [release record](../releases/v0.5.0.md).
Implementation facts are in the [capability matrix](../reference/CAPABILITY_MATRIX.md).
This directory holds only incomplete work.

## What remains

### The move is not finished until the copies are gone ⬜

A library that lives here and also still lives in `usd-vrm-plugins` is two
libraries. Each consumer deletes its copy in its own change, against this
release:

- ⬜ **`usd-vrm-plugins` switches its nine members and deletes them**
  (its MIG-1 to MIG-4). It pins `>=0.5,<0.6` with the digests this release
  publishes. That change is also where the renames it inherits land —
  `Humanoid.h` is `MotionPose.h` here, a pose's `source` is its `metadata`,
  and each exec node becomes one library call.
- 🚧 **`execMotion` is the last of them, and v0.5.1 publishes it**
  ([its record](../releases/v0.5.1.md), prepared 2026-09-24). v0.5.0 attached
  the bundle and the CLIs without pushing them, so no consumer could pin
  them. v0.5.1 pushes both, and `ost` 0.23.4 lets a consumer pin them. The
  deletion also waits on `ost`: its root build materializes external libraries
  only, so the consumer's root CTest suites that compose the bundle would lose
  it.
- ⬜ **The parity evidence its MIG-0 named is reproduced against these
  packages** before those deletions: 414 598 compared values at its v0.9.0,
  every one `==`. Reproducing it is what makes the move provably
  behaviour-preserving rather than merely compiling. Two changes here are
  known to move a value and are expected to show: `RootMotion::worldOrientation`
  is carried now, where both copies there dropped it, and a clip-sourced pose
  carries a filter policy it did not have.
- ⬜ **`usd-mmd-plugins`' `mmdMotionAdapter` configures against the installed
  `motionCore`** — the first consumer that has never heard of VRM, and the
  only test of whether the vocabulary is actually format-neutral rather than
  VRM's with the names changed.
- ⬜ **`motion-connectors` consumes `motionCore` and `motionRecording`**, which
  its remaining imports (`motionTracking` and the three adapters) all link.

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

This milestone is done when no consumer holds a second copy of anything
published here, and the parity evidence has been reproduced across the move.
