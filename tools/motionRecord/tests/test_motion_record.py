#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""End-to-end check for motion_record, the replay tool.

It replays recorded sessions into motion stages and reads them back through
OpenUSD: the stage's shape and provenance, the intake policies, a transport
that falls behind, and the two root-motion cases the tool has regressed on
before. It finishes by resolving the stage through a `UsdSkelSkeletonQuery`,
because a clip that binds and animates nothing passes every other check.

In usd-vrm-plugins, where this tool was `motion_capture`, the last step baked
the clip onto a VRM avatar with `motion_retarget` first. That leg is a consumer
of this repository and stays there; what travels is the claim it rested on,
that the stage resolves and moves.
"""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import subprocess
import sys
import tempfile

from pxr import Gf, Sdf, Usd, UsdSkel

TOLERANCE = 1e-5


class Failures:
    def __init__(self) -> None:
        self.messages: list[str] = []

    def check(self, condition: bool, message: str) -> bool:
        if not condition:
            self.messages.append(message)
        return condition

    def report(self) -> int:
        if not self.messages:
            return 0
        for message in self.messages:
            print(f"FAIL: {message}", file=sys.stderr)
        return 1


def run_tool(tool: str, *arguments: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [tool, *arguments], text=True, encoding="utf-8", errors="replace",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def find_animation(stage: Usd.Stage) -> UsdSkel.Animation:
    for prim in stage.Traverse():
        if prim.IsA(UsdSkel.Animation):
            return UsdSkel.Animation(prim)
    raise AssertionError(
        f"{stage.GetRootLayer().identifier} has no SkelAnimation")


def find_skeleton(stage: Usd.Stage) -> UsdSkel.Skeleton:
    for prim in stage.Traverse():
        if prim.IsA(UsdSkel.Skeleton):
            return UsdSkel.Skeleton(prim)
    raise AssertionError(f"{stage.GetRootLayer().identifier} has no Skeleton")


def quaternions_match(a: Gf.Quatf, b: Gf.Quatf) -> bool:
    dot = (a.GetReal() * b.GetReal()
           + Gf.Dot(a.GetImaginary(), b.GetImaginary()))
    if dot < 0.0:
        b = Gf.Quatf(-b.GetReal(), -b.GetImaginary())
    return (abs(a.GetReal() - b.GetReal()) <= TOLERANCE
            and all(abs(x - y) <= TOLERANCE
                    for x, y in zip(a.GetImaginary(), b.GetImaginary())))


def track_varies(attribute, times) -> bool:
    """True when the attribute's value changes anywhere along the timeline.

    Deliberately not a first-vs-last comparison: the walk fixtures span a whole
    number of gait cycles, so their endpoints coincide and an endpoint check
    would report a moving clip as frozen.
    """
    reference = attribute.Get(times[0])
    return any(
        any(not quaternions_match(a, b)
            for a, b in zip(reference, attribute.Get(time)))
        for time in times[1:])


def capture(tool: str, trace: pathlib.Path, output: pathlib.Path,
            *extra: str) -> subprocess.CompletedProcess:
    return run_tool(tool, "--trace", str(trace), "--output", str(output),
                    *extra)


def check_clip_shape(clip: pathlib.Path, failures: Failures) -> None:
    """The authored stage must be the motion stage (USD_MAPPING.md §2-§5)."""
    stage = Usd.Stage.Open(str(clip))
    if not failures.check(stage is not None, f"could not open {clip}"):
        return

    failures.check(
        stage.GetDefaultPrim().GetPath() == Sdf.Path("/Animation"),
        f"{clip.name}'s default prim is {stage.GetDefaultPrim().GetPath()}")
    failures.check(
        stage.GetTimeCodesPerSecond() == 30.0,
        f"{clip.name} authors {stage.GetTimeCodesPerSecond()} time codes per "
        f"second, not the mapping's 30")

    animation = find_animation(stage)
    failures.check(
        animation.GetPrim().GetPath() == Sdf.Path("/Animation/Body"),
        f"the animation is {animation.GetPrim().GetPath()}, not /Animation/Body")
    joints = list(animation.GetJointsAttr().Get() or [])
    failures.check(bool(joints), f"{clip.name} has no joints")

    # Semantic humanoid paths, not a target rig's joint names. This is the
    # invariant that keeps the clip retargetable onto any avatar.
    failures.check(
        joints[0] == "hips",
        f"{clip.name} does not start at the semantic root: {joints[:1]}")
    failures.check(
        all("/" not in joint or joint.startswith("hips/") for joint in joints),
        f"{clip.name} carries a joint outside the humanoid hierarchy")

    # scales must be authored: UsdSkel fetches translations, rotations and
    # scales as a unit, and a clip missing scales binds cleanly and then holds
    # every joint at rest (usd-vrm-plugins' v0.4.0 regression,
    # animu-sphere/usd-vrm-plugins#64).
    scales = animation.GetScalesAttr().Get()
    failures.check(
        scales is not None and len(scales) == len(joints),
        f"{clip.name} does not author scales for every joint")

    rotations = animation.GetRotationsAttr()
    times = rotations.GetTimeSamples()
    failures.check(len(times) > 1,
                   f"{clip.name} has {len(times)} rotation time sample(s)")

    # The skeleton must be bound to the animation, or nothing downstream
    # resolves.
    skeleton = find_skeleton(stage)
    targets = UsdSkel.BindingAPI(skeleton.GetPrim()) \
        .GetAnimationSourceRel().GetTargets()
    failures.check(
        targets == [animation.GetPrim().GetPath()],
        f"{clip.name} skel:animationSource is {targets}")

    # Provenance survived the whole path: the stage says it came from a
    # capture, from which source, and under which intake settings. The session's
    # fields are the `source` dictionary and the motion's are `motion`
    # (USD_MAPPING.md §5), so they are read by key path.
    root = stage.GetDefaultPrim()
    failures.check(
        root.GetCustomDataByKey("motion:sourceFormat") == "capture",
        f"{clip.name} does not record motion:sourceFormat=capture")
    failures.check(
        root.GetCustomDataByKey("source:kind") == "liveCapture",
        f"{clip.name} does not record source:kind=liveCapture")
    failures.check(bool(root.GetCustomDataByKey("source:sourceId")),
                   f"{clip.name} does not record source:sourceId")
    failures.check(
        root.GetCustomDataByKey("source:missingJoints") is not None,
        f"{clip.name} does not record the intake policy")

    # The clip has to actually move; a bound clip that holds one pose would
    # satisfy everything above. Comparing the endpoints would not do it: the
    # walk fixtures span a whole number of cycles, so their first and last
    # frames are legitimately identical.
    failures.check(track_varies(rotations, times),
                   f"{clip.name} authors the same rotation at every time "
                   f"sample")


def check_missing_joint_policies(tool: str, corpus: pathlib.Path,
                                directory: pathlib.Path,
                                failures: Failures) -> None:
    """`hold` and `unbound` must produce visibly different clips.

    The dropout fixture loses its left arm for fifteen frames. Held, the arm
    keeps its last observed rotation and stays in the clip's joint set exactly
    as before; unbound, those frames fall back to rest. A policy that made no
    difference would mean the gate never ran.
    """
    trace = corpus / "walk-dropout-30hz.trace"
    if not failures.check(trace.is_file(), f"missing fixture {trace}"):
        return

    held = directory / "dropout_held.usda"
    unbound = directory / "dropout_unbound.usda"
    for output, mode in ((held, "hold"), (unbound, "unbound")):
        result = capture(tool, trace, output, "--missing-joints", mode)
        if not failures.check(
                result.returncode == 0,
                f"motion_record --missing-joints {mode} failed: "
                f"{result.stderr.strip()}"):
            return

    held_stage = Usd.Stage.Open(str(held))
    unbound_stage = Usd.Stage.Open(str(unbound))
    held_animation = find_animation(held_stage)
    unbound_animation = find_animation(unbound_stage)

    joints = list(held_animation.GetJointsAttr().Get() or [])
    failures.check(
        joints == list(unbound_animation.GetJointsAttr().Get() or []),
        "the two policies disagree about which joints the session observed")

    try:
        hand = joints.index(next(j for j in joints
                                 if j.endswith("leftHand")))
    except StopIteration:
        failures.check(False, "the dropout fixture has no leftHand joint")
        return

    times = held_animation.GetRotationsAttr().GetTimeSamples()
    differing = sum(
        1 for time in times
        if not quaternions_match(
            held_animation.GetRotationsAttr().Get(time)[hand],
            unbound_animation.GetRotationsAttr().Get(time)[hand]))
    failures.check(
        differing > 0,
        "hold and unbound produced identical leftHand tracks; the missing-joint "
        "policy never ran")


def check_lagged_delivery_still_resolves(tool: str, corpus: pathlib.Path,
                                         directory: pathlib.Path,
                                         failures: Failures) -> None:
    """A transport running behind must degrade, not break.

    With frames delivered 100 ms behind the tick, the consumer spends the
    session holding and extrapolating. The clip must still be complete and
    animated -- that is the difference between latency and failure.
    """
    trace = corpus / "walk-clean-30hz.trace"
    output = directory / "lagged.usda"
    result = capture(tool, trace, output, "--delivery-lag", "0.1", "--report")
    if not failures.check(result.returncode == 0,
                          f"motion_record --delivery-lag failed: "
                          f"{result.stderr.strip()}"):
        return

    failures.check("evaluation:" in result.stdout,
                   "--report printed no evaluation summary")
    failures.check("peak lag:" in result.stdout,
                   "--report printed no peak lag")

    stage = Usd.Stage.Open(str(output))
    animation = find_animation(stage)
    times = animation.GetRotationsAttr().GetTimeSamples()
    failures.check(len(times) > 10,
                   f"the lagged session recorded only {len(times)} frame(s)")


def check_normalize_is_idempotent(tool: str, corpus: pathlib.Path,
                                  directory: pathlib.Path,
                                  failures: Failures) -> None:
    trace = corpus / "walk-jitter-30hz.trace"
    output = directory / "normalized.trace"
    result = run_tool(tool, "--trace", str(trace), "--normalize", str(output))
    if not failures.check(result.returncode == 0,
                          f"--normalize failed: {result.stderr.strip()}"):
        return
    failures.check(
        output.read_bytes() == trace.read_bytes(),
        "--normalize changed a corpus trace; the committed fixture is not in "
        "canonical form")


def check_root_motion_survives_without_a_hips_rotation(
        tool: str, directory: pathlib.Path, failures: Failures) -> None:
    """A rig may report a root position while never solving a hips rotation.

    Root translation is authored onto the Hips joint and nowhere else. Building
    the joint set from observed *rotations* alone therefore dropped such a
    session's entire root motion, with no error and no warning -- the clip was
    simply a rig standing still. Hips now joins the joint set on the strength of
    the root observation, with an identity rotation track.
    """
    trace = directory / "rootless_hips.trace"
    lines = ["!motion-capture-trace 1", "provider example.test",
             "protocol replay", "sourceId rootless-hips-01",
             "frameRate 30.000000"]
    for index in range(6):
        half = math.radians(4.0 * index) * 0.5
        lines += ["",
                  f"t {index / 30.0:.6f}",
                  f"root pos 0.000000 0.900000 {index * 0.05:.6f}",
                  f"b spine {math.cos(half):.6f} {math.sin(half):.6f} "
                  f"0.000000 0.000000"]
    trace.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

    output = directory / "rootless_hips.usda"
    result = capture(tool, trace, output)
    if not failures.check(
            result.returncode == 0,
            f"motion_record rejected a hips-less rig: {result.stderr.strip()}"):
        return

    stage = Usd.Stage.Open(str(output))
    animation = find_animation(stage)
    joints = list(animation.GetJointsAttr().Get() or [])
    if not failures.check(
            joints == ["hips", "hips/spine"],
            f"a hips-less rig authored {joints}, not ['hips', 'hips/spine']"):
        return

    translations = animation.GetTranslationsAttr()
    depths = [translations.Get(time)[joints.index("hips")][2]
              for time in translations.GetTimeSamples()]
    failures.check(
        max(depths) - min(depths) > 1e-4,
        "the hips translation track is flat: the session's root motion was "
        "dropped because no hips rotation was ever observed")


def check_a_frame_without_a_root_holds_the_placement(
        tool: str, directory: pathlib.Path, failures: Failures) -> None:
    """A frame that reports no root must not send the body back to the start.

    UsdSkel needs a translation for the hips at every time sample, so this
    writer has to author something for a frame whose root is absent. It used to
    author the *rest* -- the session's first observed position -- which is
    correct for a clip where no frame reports a root and wrong the moment one
    does: a single rootless frame between two that travelled teleported the body
    to wherever the session began and back, in one frame.

    A missing root is not a missing joint. The rest rotation is neutral, so
    authoring it for an unobserved joint states an absence; the rest translation
    is a *place*, so authoring it states a trip that never happened.

    Reachable from a live connector: a protocol frame can close with joint
    rotations and no root position, and a device frame whose hips record did not
    arrive composes none.
    """
    trace = directory / "root_gap.trace"
    lines = ["!motion-capture-trace 1", "provider example.test",
             "protocol replay", "sourceId root-gap-01",
             "frameRate 30.000000"]
    # Six frames walking along +Z, with the fourth reporting no root at all.
    for index in range(6):
        lines += ["", f"t {index / 30.0:.6f}"]
        if index != 3:
            lines.append(f"root pos 0.000000 0.900000 {index * 0.10:.6f}")
        lines.append("b spine 1.000000 0.000000 0.000000 0.000000")
    trace.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

    output = directory / "root_gap.usda"
    result = capture(tool, trace, output)
    if not failures.check(
            result.returncode == 0,
            f"motion_record rejected a trace with a root gap: "
            f"{result.stderr.strip()}"):
        return

    stage = Usd.Stage.Open(str(output))
    animation = find_animation(stage)
    joints = list(animation.GetJointsAttr().Get() or [])
    translations = animation.GetTranslationsAttr()
    times = translations.GetTimeSamples()
    depths = [translations.Get(time)[joints.index("hips")][2] for time in times]
    if not failures.check(len(depths) == 6,
                          f"the clip carries {len(depths)} sample(s) of 6"):
        return

    # The rootless frame holds its predecessor's 0.20 rather than dropping to
    # the session's first position, 0.00. Asserted as the *value* and not as
    # "did not decrease", because the sequence is monotonic and a hold is the
    # only thing that keeps it so.
    failures.check(
        abs(depths[3] - depths[2]) < 1e-6,
        f"the frame reporting no root authored {depths[3]:.6f} where the frame "
        f"before it reported {depths[2]:.6f}; a rootless frame is an absence to "
        f"hold, not a return to the session's origin")
    # And the frames on either side are untouched, so the hold did not become a
    # smoothing that flattens the walk.
    failures.check(
        abs(depths[4] - 0.40) < 1e-6 and abs(depths[2] - 0.20) < 1e-6,
        f"holding the rootless frame changed its neighbours: {depths}")


def check_malformed_trace_is_rejected(tool: str, directory: pathlib.Path,
                                      failures: Failures) -> None:
    bad = directory / "bad.trace"
    bad.write_text("!motion-capture-trace 1\nt 0.0\nb elbow 1 0 0 0\n",
                   encoding="utf-8", newline="\n")
    result = capture(tool, bad, directory / "never.usda")
    failures.check(result.returncode != 0,
                   "motion_record accepted a trace naming an unknown joint")
    failures.check("elbow" in result.stderr,
                   f"the rejection does not name the offending joint: "
                   f"{result.stderr.strip()}")


def check_channels_reach_the_stage(tool: str, corpus: pathlib.Path,
                                   directory: pathlib.Path,
                                   failures: Failures) -> None:
    """A session's channels are authored, keyed by their semantic.

    Until USD-O4 was decided the stage had no `Channels` prim and a recorded
    face was dropped with a warning. It is authored now (USD_MAPPING.md §4.3),
    so the check is what reached the stage: one prim per channel, the semantic
    verbatim on `motion:channelName`, and a value keyed at the frames that
    reported it.
    """
    trace = corpus / "expressions-30hz.trace"
    if not failures.check(trace.is_file(), f"missing fixture {trace}"):
        return
    output = directory / "expressions.usda"
    result = capture(tool, trace, output)
    if not failures.check(result.returncode == 0,
                          f"motion_record failed on {trace.name}: "
                          f"{result.stderr.strip()}"):
        return

    stage = Usd.Stage.Open(str(output))
    channels = stage.GetPrimAtPath("/Animation/Channels")
    if not failures.check(bool(channels),
                          f"{trace.name} carries channels and the stage has no "
                          f"/Animation/Channels prim"):
        return
    names = {}
    for prim in channels.GetChildren():
        attribute = prim.GetAttribute("motion:channelName")
        if not failures.check(bool(attribute),
                              f"channel {prim.GetPath()} states no "
                              f"motion:channelName"):
            continue
        names[attribute.Get()] = prim
    failures.check(len(names) > 0,
                   f"{trace.name} authored no channel under /Animation/Channels")
    failures.check(
        str(len(names)) + " channel(s)" in result.stderr,
        f"motion_record did not report the {len(names)} channel(s) it wrote: "
        f"{result.stderr.strip()!r}")
    for name, prim in names.items():
        value = prim.GetAttribute("motion:channelValue")
        if not failures.check(bool(value),
                              f"channel '{name}' states no motion:channelValue"):
            continue
        failures.check(
            len(value.GetTimeSamples()) > 0,
            f"channel '{name}' is authored with no time sample at all")

    quiet = capture(tool, corpus / "walk-clean-30hz.trace",
                    directory / "no_channels.usda")
    failures.check(
        "0 channel(s)" in quiet.stderr,
        f"a session without channels did not report zero of them: "
        f"{quiet.stderr.strip()!r}")
    failures.check(
        not Usd.Stage.Open(
            str(directory / "no_channels.usda")).GetPrimAtPath(
                "/Animation/Channels"),
        "a session without channels authored a Channels prim")


def check_stage_resolves_and_moves(clip: pathlib.Path,
                                   failures: Failures) -> None:
    """The stage resolves through UsdSkel, and the skeleton it drives moves."""
    # Both stay in locals: the query holds no strong reference back, so a
    # temporary would be released out from under it.
    stage = Usd.Stage.Open(str(clip))
    skeleton = find_skeleton(stage)
    cache = UsdSkel.Cache()
    query = cache.GetSkelQuery(skeleton)
    if not failures.check(bool(query),
                          f"{clip.name} yields no UsdSkel skeleton query"):
        return

    animation = find_animation(stage)
    times = animation.GetRotationsAttr().GetTimeSamples()
    if not failures.check(len(times) > 1,
                          f"{clip.name} has {len(times)} time sample(s)"):
        return

    def rotations_at(time) -> list[Gf.Quatf]:
        transforms = query.ComputeJointLocalTransforms(Usd.TimeCode(time))
        if transforms is None:
            return []
        quaternions = []
        for transform in transforms:
            rotation = transform.ExtractRotationQuat()
            quaternions.append(
                Gf.Quatf(rotation.GetReal(),
                         Gf.Vec3f(*rotation.GetImaginary())))
        return quaternions

    reference = rotations_at(times[0])
    if not failures.check(
            len(reference) > 0,
            f"UsdSkel resolved no joint transforms from {clip.name}: the "
            f"animation is bound but does not drive the rig"):
        return

    # The rig must actually move over the session. A clip that binds and then
    # holds the rest pose resolves fine and animates nothing -- exactly
    # usd-vrm-plugins' v0.4.0 regression (animu-sphere/usd-vrm-plugins#64),
    # and the only check that catches it. Sampled
    # across the whole timeline, not endpoint to endpoint: the walk fixtures
    # span whole gait cycles and legitimately return to their opening pose.
    moved = any(
        any(not quaternions_match(a, b)
            for a, b in zip(reference, rotations_at(time)))
        for time in times[1:])
    failures.check(
        moved,
        f"UsdSkel resolves {clip.name} to the same pose at every time: the "
        f"recorded session binds but does not animate the skeleton")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", required=True)
    parser.add_argument("--corpus", required=True)
    options = parser.parse_args()

    corpus = pathlib.Path(options.corpus).resolve()
    if not corpus.is_dir():
        print(f"FAIL: corpus directory {corpus} does not exist", file=sys.stderr)
        return 1

    failures = Failures()
    with tempfile.TemporaryDirectory() as name:
        directory = pathlib.Path(name)

        clip = directory / "captured.usda"
        result = capture(options.tool, corpus / "walk-clean-30hz.trace", clip,
                         "--report")
        if not failures.check(
                result.returncode == 0,
                f"motion_record failed: {result.stderr.strip()}"):
            return failures.report()

        check_clip_shape(clip, failures)
        check_missing_joint_policies(options.tool, corpus, directory, failures)
        check_lagged_delivery_still_resolves(options.tool, corpus, directory,
                                             failures)
        check_normalize_is_idempotent(options.tool, corpus, directory, failures)
        check_root_motion_survives_without_a_hips_rotation(
            options.tool, directory, failures)
        check_a_frame_without_a_root_holds_the_placement(
            options.tool, directory, failures)
        check_malformed_trace_is_rejected(options.tool, directory, failures)
        check_channels_reach_the_stage(options.tool, corpus, directory,
                                       failures)
        check_stage_resolves_and_moves(clip, failures)

    return failures.report()


if __name__ == "__main__":
    raise SystemExit(main())
