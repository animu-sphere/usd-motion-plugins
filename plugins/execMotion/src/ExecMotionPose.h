// SPDX-License-Identifier: Apache-2.0
//
// The poses this bundle's computations produce, over plain values.
//
// This is the bundle's project-owned seam and it takes **plain values**: joint
// paths, rotation and translation arrays, a frame and a rate -- never a
// `VdfContext` and never a stage. Marshalling exec's inputs into these arguments
// is the registration TU's job (design policy §21: a computation is a thin
// wrapper over a library that does not know exec exists). Keeping the seam free
// of exec means it is testable with no stage, no system, and no request -- and
// that a failure in the mechanism cannot be mistaken for a failure in the value.
#pragma once

#include <motionCore/MotionPose.h>
#include <motionSampling/Blend.h>
#include <motionSampling/Filter.h>
#include <motionRecording/LiveCaptureSource.h>
#include <motionSampling/MotionSource.h>
#include <motionUsd/ClipReader.h>

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec3f.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <string>
#include <vector>

namespace execmotion
{

/// The identity pose for `jointPaths`.
///
/// Every rotation is identity and `validRotations` carries exactly the joints the
/// joint paths name.
///
/// The pose's `timestamp` is left at zero, and the caller does not get to pass
/// one. This computation is the same pose at every frame, so it declares no time
/// input at all (see ExecMotionRegistration.cpp).
///
/// No sampling, no interpolation, no retarget. This is the identity, and it is
/// what makes the first OpenExec computation attributable: a wrong result is a
/// wrong mechanism, because there is no algorithm to blame.
openstrata::motion::MotionPose IdentityPoseForJoints(const std::vector<std::string>& jointPaths);

/// What a `UsdSkelAnimation` states at one instant, and the pose it states:
/// `motionUsd`'s, not this bundle's.
///
/// `openstrata::motion::MotionStageSample` and `PoseFromStageSample` are the
/// rule, and this bundle calls them. It used to carry its own copy, because
/// the rule lived in `usd-vrm-plugins`' retarget CLI and a computation cannot
/// call a CLI; the reading half moved into `motionUsd` on 2026-09-20 and the
/// copy went with it (USD_MAPPING.md section 7, its OpenExec sampling
/// finding).
///
/// Two things the shared rule states that the copy did not, and they change
/// what this bundle answers -- see the changelog and USD_MAPPING.md section 7:
///
/// - The hips rotation is `RootMotion::worldOrientation` as well as the local
///   rotation (MOTION_CONTRACT.md section 5.3). The copy left
///   `root.hasOrientation` false on the reasoning that a `UsdSkelAnimation`
///   states no separate root orientation, which the contract overrules: the
///   duplication is the record, not an encoding accident.
/// - A clip of exactly one joint keeps the fallback this bundle measured. An
///   unauthored `rotations` or `translations` reaches a callback as ONE
///   element of Sdf's fallback rather than as nothing, so against one joint it
///   pairs and a hips-only clip that keys nothing samples to a root at the
///   origin. `PoseFromStageSample` states it; `execMotion_sample` pins it.
///
/// `rotations` and `translations` are already resolved **at** `timeCode`: exec
/// resolves a time-sampled attribute input at the time the computation is
/// evaluated at, so this layer interpolates nothing and holds nothing. That is
/// the one behavioural difference from `openstrata::motion::SampleAnimation`,
/// which is handed a whole `MotionClip` and does its own hold-at-the-edges
/// lookup; which of the two answers a frame between keys is USD's question here
/// and `motionSampling`'s there, and usd-vrm-plugins' parity rows are where
/// the two get compared.
using openstrata::motion::MotionStageSample;
using openstrata::motion::PoseFromStageSample;

/// What a clip states about how it wants to be smoothed.
///
/// Every field is optional and an absent one is **not** a value this bundle
/// picks: it is left at `openstrata::motion::PoseFilter::Options`' own default,
/// because a wrapper that supplied its own default would be a second policy
/// sitting on top of the library's, and a clip authoring nothing would then be
/// smoothed differently here than by the same library called anywhere else.
///
/// That is a different judgement from the sampling rate, and the difference is
/// what each absent value costs. A missing rate produces a *number* -- a
/// `timestamp` in seconds -- that no consumer can tell from a measured one. A
/// missing cutoff selects the library's documented behaviour, which every
/// caller of `openstrata::motion::PoseFilter` already gets. So the rate is
/// refused and these are defaulted (the sampling report's "every node owes its
/// own refusal" applies to what a node cannot compute without, and this one
/// can).
struct FilterPolicy
{
    /// `motion:filter:cutoffHz`. Non-positive disables smoothing, which is
    /// `openstrata::motion::PoseFilter`'s own documented pass-through and not a
    /// special case this layer added.
    std::optional<float> cutoffHz;

    /// `motion:filter:rootPosition` / `motion:filter:rootOrientation`.
    ///
    /// The orientation flag **used to be inert** over a clip-sourced pose,
    /// because the copy of the sampling rule this bundle carried never set
    /// `root.hasOrientation` and `PoseFilter` skips a field the pose does not
    /// carry. Since `PoseFromStageSample` (2026-09-20) a clip that turns its
    /// hips carries a root orientation, so the flag smooths one: turning it on
    /// now changes what this node answers, where before it changed nothing.
    std::optional<bool> filterRootPosition;
    std::optional<bool> filterRootOrientation;
};

/// `pose` smoothed against `prior`, under `policy`.
///
/// One step of `openstrata::motion::PoseFilter`, and the state it needs is
/// passed in rather than kept. That is forced rather than chosen: an OpenExec
/// callback is handed exactly one time and no way to reach another ([the
/// sampling report](https://github.com/animu-sphere/usd-vrm-plugins/blob/main/docs/reports/openusd/26.08-openexec-sampling.md)
/// §5), and a filter that remembered the last pose in a static would be the
/// mutable state the purity rule forbids and invalidation cannot see. So the
/// recurrence
/// -- "the prior pose is the previous frame's answer" -- belongs to whoever
/// drives the graph, which is where it already lives for a live source:
/// `motionSampling`'s pose buffer.
///
/// Seeding is the library's own: a `PoseFilter` with no state returns its first
/// pose unchanged and keeps it, so `prior` costs one `Apply` and no special
/// case. Two consequences fall out rather than being written: passing the same
/// pose as both arguments returns it unchanged (`dt` is zero), and so does a
/// `prior` stamped at or after `pose` -- a seek backwards is a reseed, exactly
/// as it is for a streamed source.
///
/// **What the round trip through a pose costs, measured rather than assumed.**
/// `PoseFilter` retains a state strictly richer than the pose it returns: a joint
/// a pose does not report keeps its stored rotation in the *state* and stays out
/// of the *result*, so a brief dropout does not restart that joint's history.
/// Only a result can travel back in as the next `prior`, so that retained half
/// does not survive the trip, and a joint returning after a missing frame is
/// passed through here where the streaming filter would slerp it -- 45 degrees
/// against 23.8 in the case `execMotion_pose` pins, in both directions.
///
/// It costs nothing for a clip, whose `joints` are `uniform` so no joint
/// ever drops out, and it is real for a live source, which is what this node
/// is aimed at. The library hands the state back -- `StepResult::state`, which
/// is what this bundle's ask on the one-step entry point was for -- so the
/// carry-forward rule is not reproduced here. What is still lost is lost in
/// the *graph*: an exec computation's value is a pose, so until a node
/// publishes the state as a value of its own, a driver has only the result to
/// hand back as the next `prior`.
openstrata::motion::MotionPose FilteredPose(const openstrata::motion::MotionPose& prior,
                                            const openstrata::motion::MotionPose& pose,
                                            const FilterPolicy& policy);

/// What a clip states about how its root is taken in.
///
/// The vocabulary is the library's -- `openstrata::motion::RootMotionIntake`,
/// the same enum a live session configures `motionRecording` with -- rather
/// than a second one spelled for exec. A wrapper that named its own policies
/// would be a wrapper over a contract of its own making.
///
/// Absent means the library's own default, the way `FilterPolicy`'s fields do,
/// and for the same reason: `LiveCaptureConfig::rootMotion` already answers this
/// for every other caller and a default invented here would answer it
/// differently. The default is *read from* `LiveCaptureConfig` rather than
/// restated (see RootMotionFrom), so the day the library moves it, this bundle
/// moves with it.
struct RootPolicy
{
    /// `motion:root:intake`. Nullopt is `LiveCaptureConfig`'s own default.
    std::optional<openstrata::motion::RootMotionIntake> intake;
};

/// The intake policy `token` names, or nullopt for a token this layer does not
/// recognize.
///
/// Absent and unrecognized are different answers, and this is where the
/// difference is drawn: an absent attribute is a clip that said nothing and gets
/// the library's default; a token that spells no policy is a clip that *stated*
/// something this layer cannot honour, and the caller refuses it. Falling back
/// to the default there would give a clip asking for `Ignore` -- misspelled --
/// the root motion it asked not to have.
///
/// The spellings are the enum's own names in lowerCamelCase, which is what a
/// USD token attribute reads like: `passthrough`, `ignore`, `deriveVelocity`.
std::optional<openstrata::motion::RootMotionIntake> RootIntakeForToken(std::string_view token);

/// The root motion `pose` states, under `policy`, given the pose before it.
///
/// `Ignore` yields a default-constructed `openstrata::motion::RootMotion` --
/// every presence flag clear, which is what "this clip's placement is not the
/// capture's to decide" looks like downstream. `Passthrough` yields the pose's
/// own root unchanged. `DeriveVelocity` is `Passthrough` plus one thing: when
/// the pose carries a position and no linear velocity and `prior` carries a
/// position, the velocity is the distance between the two over the seconds
/// between them.
///
/// `prior` is the same value `motion.filterPose` takes, and it reaches this
/// function under the same rule: a computation is handed one instant and a
/// velocity needs two, so the previous frame's answer is supplied by whoever
/// drives the graph. Un-overridden it is the pose itself, the elapsed time is
/// zero, and no velocity is derived -- the pose passes through, which is the
/// same shape a zero-length filter step has and is not special-cased here
/// either.
///
/// **The whole node, and it is a wrapper**: `ConditionRootMotion`, the intake
/// rule as a pure function beside the capture session that applies it to every
/// frame it accepts. So every rule in the answer is the library's, and the rule
/// has one implementation again.
///
/// It was this bundle's third "not a wrapper" finding before the move: the
/// rule lived in a **private** method of `LiveCaptureSource`, a capture
/// *session* that owns a pose buffer, a filter, held-joint state and
/// statistics and refuses a frame whose timestamp does not increase, so there
/// was no call to make and the three conditions were reproduced here. The
/// seed-then-step idiom `FilteredPose` used does not transfer to a session:
/// two poses at the same instant are one accepted frame and one refusal, and
/// the buffer head is then the prior rather than the pose.
openstrata::motion::RootMotion RootMotionFrom(const openstrata::motion::MotionPose& prior,
                                              const openstrata::motion::MotionPose& pose,
                                              const RootPolicy& policy);

/// The transform `root` places something at, as a local-to-parent matrix: the
/// orientation, then the position, each only where the root states it.
///
/// It is what `motion:root:transform` computes, and so what an Xformable whose
/// `xformOp:transform` is connected to that attribute draws at through
/// `usdExecImaging` (the display node). An unstated component contributes
/// nothing rather than a guess, so a cleared root -- `ignore`'s answer -- is the
/// identity, which leaves the Xformable at its parent's placement: the
/// "leave the rig its own placement" that `ignore` exists to say.
///
/// Nullopt for a root this layer cannot turn into a matrix, and the caller
/// refuses it: a position or orientation that is not finite, or an orientation
/// too short to normalize. A matrix built from either would be a transform
/// nobody stated -- NaN hides the prim, and a normalized near-zero quaternion is
/// an arbitrary rotation -- and a velocity is not read at all, because a
/// placement at one instant does not depend on one.
std::optional<pxr::GfMatrix4d> RootTransform(const openstrata::motion::RootMotion& root);

/// The history a driver's pose buffer holds, when no driver supplies one: the
/// pose at the evaluated instant, as a one-sample
/// `openstrata::motion::MotionClip`.
///
/// It is `motion.poseHistory`'s ordinary value and it exists for the same reason
/// `motion.priorPose`'s does -- to be *replaced*. A computation evaluates an
/// immutable snapshot and never reaches for one (design policy §21), so a live
/// source's buffered samples reach the graph the one way a value the scene does
/// not state can: as an override on a value key.
///
/// The span is the one instant, `startTime == endTime == pose.timestamp`, because
/// that is what a history of one sample spans. `nominalFrameRate` is left at the
/// library's own default rather than set: a single sample has no rate to state,
/// and the node that reads this value does not use one.
openstrata::motion::MotionClip HistoryOfOne(const openstrata::motion::MotionPose& pose);

/// What `history` states at `seconds`, or nullopt when it cannot be sampled.
///
/// **The whole node, and it is a wrapper**: `openstrata::motion::SampleClip`
/// over the history. That is `IMotionSource`'s one question -- "what is the
/// pose at this evaluation time?" -- answered from a clip held by reference, so
/// nothing is constructed and nothing is copied. So every rule in the answer is
/// the library's: bracketing samples are interpolated by
/// `openstrata::motion::LerpPose` (a missing joint held, never faded), a time
/// outside the history holds the nearer boundary, and the pose comes back
/// stamped at `seconds`, on the consumer's clock.
///
/// The result is the library's `openstrata::motion::PoseSampleResult` and not a
/// bare pose, because the status is part of the answer (motion contract,
/// live-capture semantics). The answer is stamped at the evaluated instant
/// *whether or not* the history reached it, so a pose alone cannot say whether
/// it was sampled or held -- and a source that has stopped delivering keeps
/// answering `Held` forever, which a consumer holding only the pose would read
/// as live. A wrapper does not get to drop a field of the thing it wraps.
///
/// An **empty** history is an answer, not a refusal: the library's
/// `Unavailable`, carrying no pose. The bundle refuses where an answer would be
/// indistinguishable from one it measured, and this type has an absent state of
/// its own, so there is nothing to refuse -- the first result in the bundle for
/// which that is true.
///
/// The one refusal is a history whose timestamps are **not finite** or
/// **decrease** somewhere. That is `SampleClip`'s stated precondition, and it
/// says the caller checks it: the search is binary, so a history out of order,
/// or carrying a NaN every comparison is false against, would answer with a
/// bracket nobody measured.
///
/// Repeated timestamps are *not* refused, because the library's answer to them
/// never leaves the samples it holds: a request at or past the end holds the
/// **last** of a repeated pair there (`samples.back()`), one at or before the
/// start holds the first, and one inside lands exactly on the first of a pair
/// or between two adjacent samples. Which of two same-instant samples answers
/// depends on where the request falls, and every answer is a measured sample or
/// an interpolation between neighbours. A check stricter than that would be a
/// policy of this bundle's -- `openstrata::motion::PoseBuffer::Push`'s
/// strictly-increasing rule is a property of how a buffer is *filled*, not of
/// what can be sampled.
std::optional<openstrata::motion::PoseSampleResult>
SampleHistory(const openstrata::motion::MotionClip& history, double seconds);

/// What a blend was handed, as plain values.
///
/// A blend is the one node in this bundle that wants poses from **several
/// places**, and it reaches them through a relationship: `motion:blend:sources`
/// targets the clips, and each target's `motion.sampleAnimation` arrives as one
/// value of a fan-in. 26.08 gives that fan-in two silent behaviours, both read
/// in `exec/inputResolver.cpp` and `vdf/readIterator.h` and both measured in
/// `execMotion_blend`:
///
///   * a target that does not provide the computation -- not a
///     `UsdSkelAnimation`, or not a prim at all -- is **skipped** while the
///     network is compiled, with no error; and
///   * a source whose computation **refused** sets no value, and the read
///     iterator skips an input that provides none, with no error either.
///
/// So the poses alone cannot say how many sources there were, and a weight
/// paired with them by position would silently land on the wrong clip the
/// moment one dropped out. `sourceCount` is what closes that: the same
/// relationship read a second time for exec's builtin `computePath`, which every
/// object that exists provides, so it counts the targets a pose *should* have
/// come back from.
struct BlendInputs
{
    /// How many objects `motion:blend:sources` reaches -- one `computePath` per
    /// target that names something on the stage. A target naming nothing is
    /// missing from this count and from `poses` alike, so it is invisible here;
    /// the weights then disagree with the count, which is how it surfaces.
    std::size_t sourceCount = 0;

    /// The poses that came back, in the order the fan-in delivered them --
    /// measured to be the relationship's authored target order.
    std::vector<openstrata::motion::MotionPose> poses;

    /// `motion:blend:weights`, as authored: one per target, in target order.
    std::vector<float> weights;
};

/// Why a blend was refused. Each is a statement the registration TU turns into
/// an error naming the computation; the seam decides, the TU reports.
enum class BlendRefusal
{
    /// The relationship reaches nothing, so there is nothing to blend and no
    /// instant to stamp a blend at.
    NoSource,

    /// Fewer poses came back than there are targets: one of them is not a clip,
    /// or its sampler refused. Which one cannot be told from here -- the fan-in
    /// hands back values, not the objects they came from.
    SourceUnanswered,

    /// The weights do not pair one-to-one with the targets. An **absent**
    /// weights attribute lands here too, deliberately: a callback cannot tell an
    /// absent array from an authored empty one (both are an iterator already at
    /// its end), so defaulting the one would default the other -- a clip that
    /// *stated* no weights would be blended evenly.
    WeightCount,

    /// A weight that is not finite. `openstrata::motion::BlendPoses` would take
    /// a NaN through its running total and answer NaN rotations.
    WeightNotFinite,

    /// The sources were not stamped at one finite instant. Every source is
    /// sampled at the same frame, so this is two clips counting that frame at
    /// different rates -- or a driver's override stamped somewhere else -- and
    /// the library would interpolate the timestamps into a second nobody
    /// measured.
    InstantsDisagree,

    /// No weight is positive. The library answers nullopt rather than a pose,
    /// and this is that answer named: there is nothing to blend.
    NothingWeighted,
};

/// The pose `inputs` blend to, or the reason there is none.
struct BlendOutcome
{
    std::optional<openstrata::motion::MotionPose> pose;
    BlendRefusal refusal = BlendRefusal::NoSource;
};

/// `openstrata::motion::BlendPoses` over `inputs`, or a refusal.
///
/// **The whole node, and it is a wrapper**: the N-way `BlendPoses`, handed each
/// pose with the weight authored at the same position. So every rule in the
/// answer is the library's -- a negative weight counts as zero, a joint only
/// some sources report is taken from those rather than blended toward identity
/// (`openstrata::motion::LerpPose`, fold by fold), and the result is stamped at
/// the sources' own instant.
///
/// **The order is part of the answer, not only of the pairing.** The library
/// folds the poses in one at a time -- each at its share of the running total --
/// which keeps every intermediate a unit quaternion, and which makes a blend of
/// three or more rotations about different axes depend on the order they are
/// folded in. So the fan-in's order has to be the authored one for two reasons:
/// the weights are paired by it, and the library's answer is computed along it.
/// `execMotion_pose` measures the second; `execMotion_blend` measures that 26.08
/// delivers the first.
///
/// Every refusal is somewhere the library would answer a pose no consumer
/// could tell from a measured one, or could not be asked at all -- the
/// bundle's refusal rule, applied six ways (`BlendRefusal`). Three of the six
/// are the library's own statements rather than this layer's: nothing weighted
/// is its nullopt, named here; and a weight that is not finite and sources
/// that disagree about the instant are the two preconditions the N-way
/// `BlendPoses` states for its caller to check.
BlendOutcome BlendedPose(const BlendInputs& inputs);

} // namespace execmotion
