# motion_record

Replays a recorded capture session into a motion stage. The trace is pushed
frame by frame into a `LiveCaptureSource`, evaluated on a fixed tick, recorded
back into a `MotionClip`, and authored through `motionUsd` as the standalone
motion stage ([USD_MAPPING.md](../../docs/design/USD_MAPPING.md) §2–§5). That
is the stage `motion_convert` authors from a recorded file, so a retargeter
reads a replayed session without knowing it was live.

It arrived from usd-vrm-plugins as `motion_capture`, with its history, and
takes this repository's command name
([WORKSPACE.md §1.2](../../docs/architecture/WORKSPACE.md#12-bundles-tools-and-data)).
It is an executable, not a bundle: it registers nothing with OpenUSD.

## The loop

```cpp
sender.Advance(now - deliveryLag);      // what has arrived by now
recorder.Record(source.Sample(now));    // what the consumer sees now
```

That is the whole of live capture as this repository defines it. A connector
replaces the first line with a decoded packet and nothing else changes, which
is why replaying a recording is a faithful test rather than a mock. Nothing in
`motionRecording` or in this tool reads a wall clock or opens a transport
([MOTION_CONTRACT.md §9–§10](../../docs/design/MOTION_CONTRACT.md)).

## Use

```sh
# Replay a session into a motion stage.
motion_record --trace session.trace --output clip.usda --report
```

### Simulating a transport that falls behind

`--delivery-lag` makes a replay behave like a real session: frames arrive that
many seconds after the tick they belong to, so the consumer holds and
extrapolates exactly as it would when the network is slow.

```sh
motion_record --trace session.trace --output clip.usda \
              --delivery-lag 0.1 --extrapolation 0.05 --report
```

`--report` prints how many frames were accepted or refused and why, how much of
the humanoid was observed, how many joints were gated or held, and how many
ticks were sampled, held, extrapolated or unanswerable. The same numbers are
written into the stage's `customData.source`, so a result can be traced back to
the session and the intake settings that produced it.

### Intake policy

| Flag | Effect |
| --- | --- |
| `--confidence-floor F` | Joints reporting below `F` are treated as missing. Frames carrying no confidence at all are never gated. |
| `--missing-joints hold\|unbound` | A missing joint keeps its last observed rotation, or is left for the target rig's rest pose. |
| `--root-motion derive\|passthrough\|ignore` | `derive` fills in a linear velocity from consecutive frames when the source reports none, which is what lets extrapolation hide a late frame. |
| `--smoothing HZ` | Frame-rate-independent exponential smoothing on intake. |

`--normalize out.trace` rewrites a trace in canonical form and exits: the way
to bring a hand-written fixture into the shape `motionRecording`'s writer
emits.

## What it authors

`/Animation`, with `/Animation/Skeleton` over semantic joint paths
(`hips/spine/chest/...`) and `/Animation/Body` bound to it, at 30 time codes
per second. `customData.motion.sourceFormat` is `capture`. The rules are
`motionUsd`'s, and two of them matter most here:

- **Rests are identity, except the hips.** A capture reports rotations relative
  to the humanoid rest, never a rest of its own, so the retargeter's rest-pose
  correction is a no-op. The hips rest translation is the session's first
  observed root position, so root motion arrives downstream as a delta from
  where the capture started.
- **A joint the session never observed is absent**, not authored at rest: a
  joint that is present and unmoving means something different downstream
  from one that was never captured.

Channels (a face's expression weights) and look-at targets are not authored
yet: the `Channels` prim waits on USD-O4. The tool says so on stderr when a
session carried either, rather than dropping them without a word.

## Tests

```sh
ctest -R motion_record_replay
```

The test replays `motionRecording`'s corpus, checks the stage's shape and
provenance, proves that the two missing-joint policies differ, drives a lagged
session, checks the two root-motion cases the tool regressed on before, and
resolves the result through a `UsdSkelSkeletonQuery` to prove that it moves.

In usd-vrm-plugins the last step baked the clip onto a VRM avatar with
`motion_retarget` first. That leg is a consumer of this repository, and it
stays there.
