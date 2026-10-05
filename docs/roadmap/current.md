# Current

What remains after the imports. Every identity planned for the first
releases is published as a digest-pinned artifact, and every consumer that
held a copy has deleted it. The release history is in the
[changelog](../../CHANGELOG.md) and the [release records](../releases/);
implementation facts are in the [capability matrix](../reference/CAPABILITY_MATRIX.md).
This directory holds only incomplete work.

## What remains

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
[the roadmap README](README.md#open-decisions). The next consumer-facing
decision is **USD-O5** (the `Bindings` prim, which `usd-avatar-runtime` needs).
MC-O5 was resolved with `motion-connectors`' first shared connector-to-intake
test; [MOTION §9](../design/MOTION_CONTRACT.md#9-motionstream-intake) records
the public stream shape.

## Completion criteria

The next composition milestone is done when a consumer can state and read
the source/target/policy relationship without redefining the generic motion
contract or modifying either referenced asset.
