# Current — after v0.5.0

Status: **v0.5.0 is prepared** (2026-09-20,
[its record](../releases/v0.5.0.md)). Every identity the scope table numbered
v0.1.0 through v0.5.0 has arrived and is published as a digest-pinned artifact.
What remains is not more imports: it is the other side of the move, in the
repositories that consume these packages, and the evidence that the move
preserved what it moved.

The scaffold, the imports and each release's scope have left this file: they
are in the [changelog](../../CHANGELOG.md), the
[release record](../releases/v0.5.0.md) and the
[scope table](README.md#status-at-a-glance). This directory holds only
incomplete work.

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

### The first publication has two unproven steps ⬜

Both are in [the release record](../releases/v0.5.0.md) and are settled by
running it, not by deciding anything:

- ⬜ whether `GITHUB_TOKEN` may create this repository's first GHCR package,
  or a PAT with `write:packages` is needed;
- ⬜ the package's visibility flip, which is manual and one-time.

### Beyond the imports ⬜

The scope table's `later` row, unchanged: the generic NPZ payload contract
(design policy §28) and the recorded-source identity decision that travels with
it, IK-assisted retarget, contacts, blending beyond the imported one, the
generator interfaces `motion-connectors`' ARDY adapter needs, and Python
bindings. None of them is started, and none blocks a consumer.

## Open decisions

They are listed, in the order they block work, in
[the roadmap README](README.md#open-decisions). The ones a consumer is most
likely to reach first are **MC-O5** (`MotionStream`'s public shape, which
`motion-connectors` needs) and **USD-O5** (the `Bindings` prim, which
`usd-avatar-runtime` needs).

## Completion criteria

This milestone is done when no consumer holds a second copy of anything
published here, and the parity evidence has been reproduced across the move.
