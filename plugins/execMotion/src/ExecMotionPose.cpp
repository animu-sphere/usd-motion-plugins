// SPDX-License-Identifier: Apache-2.0
#include "ExecMotionPose.h"

#include "pxr/base/gf/limits.h"
#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/vec3d.h"

#include <cmath>
#include <cstddef>
#include <string_view>

namespace execmotion
{

std::optional<openstrata::motion::HumanJoint>
JointForPath(const std::string& jointPath)
{
    // A joint path is `parent/child/leaf`; the joint is the leaf. A path with no
    // separator is already a leaf, which is what a flat rig authors.
    const std::size_t slash = jointPath.rfind('/');
    const std::string_view leaf = slash == std::string::npos
                                      ? std::string_view(jointPath)
                                      : std::string_view(jointPath).substr(slash + 1);
    if (leaf.empty())
    {
        return std::nullopt;
    }
    return openstrata::motion::FindHumanJoint(leaf);
}

openstrata::motion::MotionPose
IdentityPoseForJoints(const std::vector<std::string>& jointPaths)
{
    openstrata::motion::MotionPose pose;
    // MotionPose's default constructor already fills localRotations with the
    // identity quaternion and clears validRotations, so this loop only says
    // which joints the clip named -- it authors no rotation at all.
    for (const std::string& jointPath : jointPaths)
    {
        if (const std::optional<openstrata::motion::HumanJoint> joint = JointForPath(jointPath))
        {
            pose.validRotations.set(static_cast<std::size_t>(*joint));
        }
    }
    return pose;
}

std::optional<openstrata::motion::MotionPose>
PoseFromClipSample(const ClipSample& sample)
{
    if (!(sample.timeCodesPerSecond > 0.0))
    {
        return std::nullopt;
    }

    openstrata::motion::MotionPose pose;

    // The default time code is not frame zero, and this is the one place the
    // difference does not produce a wrong number: a pose resolved outside a
    // timeline carries no second, `timestamp` has no absent state, and frame
    // zero converts to 0.0 at every rate -- so "no time" and "the first frame"
    // are the same value here whatever the clip's rate is. What differs between
    // the two is which values USD resolved, and that happened before this call.
    if (sample.hasTimeCode)
    {
        pose.timestamp = sample.timeCode / sample.timeCodesPerSecond;
    }

    const std::size_t jointCount = sample.jointPaths.size();
    const bool rotationsUsable = sample.rotations.size() == jointCount;
    const bool translationsUsable = sample.translations.size() == jointCount;

    for (std::size_t i = 0; i < jointCount; ++i)
    {
        const std::optional<openstrata::motion::HumanJoint> joint =
            JointForPath(sample.jointPaths[i]);
        if (!joint)
        {
            continue;
        }
        const auto slot = static_cast<std::size_t>(*joint);

        if (rotationsUsable)
        {
            // Normalized on the way in, like the offline reader: a clip may
            // author a quaternion that has drifted off the unit sphere, and
            // every consumer of a canonical pose is entitled to a rotation.
            pose.localRotations[slot] = sample.rotations[i].GetNormalized();
            pose.validRotations.set(slot);
        }

        // Only the hips carry body translation. A `translations` array states
        // one per joint, but the rest of it is the rest pose the source rig was
        // authored with, which a retargeter re-derives for the rig it is aiming
        // at (motion contract; usd-vrm-plugins' motion_retarget reads a
        // clip the same way).
        if (translationsUsable && *joint == openstrata::motion::HumanJoint::Hips)
        {
            pose.root.worldPosition = sample.translations[i];
            pose.root.hasPosition = true;
        }
    }

    return pose;
}

openstrata::motion::MotionPose
FilteredPose(const openstrata::motion::MotionPose& prior,
             const openstrata::motion::MotionPose& pose, const FilterPolicy& policy)
{
    // The library's defaults, then whatever the clip actually stated. Each
    // field is overwritten independently, so a clip that authors a cutoff and
    // nothing else keeps the library's answer for the other two.
    openstrata::motion::PoseFilter::Options options;
    if (policy.cutoffHz)
    {
        options.cutoffHz = *policy.cutoffHz;
    }
    if (policy.filterRootPosition)
    {
        options.filterRootPosition = *policy.filterRootPosition;
    }
    if (policy.filterRootOrientation)
    {
        options.filterRootOrientation = *policy.filterRootOrientation;
    }

    // The whole node, and it is one library call: `PoseFilter::Step`, the
    // streaming filter as a pure function, handed the prior pose as the state
    // it steps from. That entry point is what this bundle asked motionSampling
    // for and it arrived with the library: seeding a local filter with `prior`
    // and applying `pose` to it answered the same pose, and had to construct an
    // object to do it.
    //
    // Only the pose travels back out. `StepResult::state` is the richer half --
    // a joint the pose did not report keeps its last smoothed rotation there --
    // and an exec computation's value is a pose, so carrying the state is a
    // driver's to do over a value key this bundle does not yet publish. The
    // difference that costs is measured in `execMotion_pose`.
    return openstrata::motion::PoseFilter::Step(&prior, pose, options).pose;
}

std::optional<openstrata::motion::RootMotionIntake>
RootIntakeForToken(std::string_view token)
{
    // Three spellings and no synonyms. A table rather than a chain of ifs
    // because the set is closed: it is `openstrata::motion::RootMotionIntake`,
    // and a fourth policy is a change to the library that has to reach this
    // list.
    if (token == "passthrough")
    {
        return openstrata::motion::RootMotionIntake::Passthrough;
    }
    if (token == "ignore")
    {
        return openstrata::motion::RootMotionIntake::Ignore;
    }
    if (token == "deriveVelocity")
    {
        return openstrata::motion::RootMotionIntake::DeriveVelocity;
    }
    return std::nullopt;
}

openstrata::motion::RootMotion
RootMotionFrom(const openstrata::motion::MotionPose& prior,
               const openstrata::motion::MotionPose& pose, const RootPolicy& policy)
{
    // The library's default, read from the library. `LiveCaptureConfig` is what
    // every other caller of this rule is configured with, so a clip that states
    // nothing gets exactly what a live session that states nothing gets --
    // including on the day that default changes.
    const openstrata::motion::RootMotionIntake intake =
        policy.intake ? *policy.intake : openstrata::motion::LiveCaptureConfig{}.rootMotion;

    // The whole node, and it is one library call: the intake rule as a pure
    // function, beside the capture session that applies it to every frame it
    // accepts. It arrived with the library (usd-motion-plugins #6); until it
    // did, the three conditions were reproduced here against the contract,
    // because the rule lived in a private method of a session that owns a
    // buffer, a filter and statistics, and there was no call to make.
    return openstrata::motion::ConditionRootMotion(prior, pose, intake);
}

std::optional<pxr::GfMatrix4d>
RootTransform(const openstrata::motion::RootMotion& root)
{
    pxr::GfMatrix4d transform(1.0);

    if (root.hasOrientation)
    {
        // In double precision before normalizing, so a unit quaternion stored in
        // float comes out as the rotation it states rather than one rounded
        // twice. The threshold is the one Gf normalizes against itself: shorter
        // than that, `GetNormalized` answers the identity -- a rotation nobody
        // stated, which is the thing this function refuses to produce.
        const pxr::GfQuatd orientation(root.worldOrientation);
        const double length = orientation.GetLength();
        if (!std::isfinite(length) || !(length > GF_MIN_VECTOR_LENGTH))
        {
            return std::nullopt;
        }
        transform.SetRotateOnly(orientation / length);
    }

    if (root.hasPosition)
    {
        const pxr::GfVec3d position(root.worldPosition);
        if (!std::isfinite(position[0]) || !std::isfinite(position[1]) ||
            !std::isfinite(position[2]))
        {
            return std::nullopt;
        }
        // Row-vector convention, as everywhere in Gf: the translation row
        // applies after the rotation above, so the orientation turns the
        // placement about its own origin and does not swing the position.
        transform.SetTranslateOnly(position);
    }

    return transform;
}

openstrata::motion::MotionClip
HistoryOfOne(const openstrata::motion::MotionPose& pose)
{
    openstrata::motion::MotionClip history;
    history.samples.push_back(pose);
    history.startTime = pose.timestamp;
    history.endTime = pose.timestamp;
    return history;
}

std::optional<openstrata::motion::PoseSampleResult>
SampleHistory(const openstrata::motion::MotionClip& history, double seconds)
{
    // The precondition the library's binary search relies on, and nothing
    // stricter: a pair of equal timestamps is something it answers, a pair that
    // goes backwards -- or a timestamp that is not a number at all -- is
    // something it would answer wrongly. Finiteness is checked on every sample
    // rather than left to the ordering comparison, because every comparison
    // with a NaN is false: `a < NaN` would let it through, and a NaN newest
    // sample would reach the caller as a NaN lag, which compares unequal even
    // to itself.
    const std::vector<openstrata::motion::MotionPose>& samples = history.samples;
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        if (!std::isfinite(samples[i].timestamp))
        {
            return std::nullopt;
        }
        if (i > 0 && samples[i].timestamp < samples[i - 1].timestamp)
        {
            return std::nullopt;
        }
    }

    // The whole node, and it is one library call: `SampleClip`, which answers
    // the question `IMotionSource::Sample` asks, from a clip held by reference.
    // It arrived with the library (usd-motion-plugins #6) and this node is why:
    // the status-carrying answer used to exist only as a method on a
    // `ClipSource`, so a pure computation had to construct one and copy the
    // history into it to get a status the contract says is part of the answer.
    return openstrata::motion::SampleClip(history, seconds);
}

BlendOutcome
BlendedPose(const BlendInputs& inputs)
{
    BlendOutcome outcome;

    // In the order that makes each reason the true one: a count can only be
    // compared once there is something to count, weights can only be judged
    // once they pair with the sources, and the instant and the total weight are
    // questions about a blend that is otherwise well formed.
    if (inputs.sourceCount == 0)
    {
        outcome.refusal = BlendRefusal::NoSource;
        return outcome;
    }
    if (inputs.poses.size() != inputs.sourceCount)
    {
        outcome.refusal = BlendRefusal::SourceUnanswered;
        return outcome;
    }
    if (inputs.weights.size() != inputs.sourceCount)
    {
        outcome.refusal = BlendRefusal::WeightCount;
        return outcome;
    }
    for (const float weight : inputs.weights)
    {
        if (!std::isfinite(weight))
        {
            outcome.refusal = BlendRefusal::WeightNotFinite;
            return outcome;
        }
    }

    // Exactly equal, not nearly: every source is converted from the same frame,
    // so two clips counting it at one rate are stamped with the same bits, and
    // any difference at all is two rates -- a tolerance here would be a policy
    // about how far apart two clocks may be. Finiteness as well as equality,
    // because equality alone lets an infinity through: sources all stamped
    // +inf agree with each other exactly. (A NaN never agrees, even with
    // itself, so equality would catch that one on its own.)
    const double instant = inputs.poses.front().timestamp;
    for (const openstrata::motion::MotionPose& pose : inputs.poses)
    {
        if (!std::isfinite(pose.timestamp) || pose.timestamp != instant)
        {
            outcome.refusal = BlendRefusal::InstantsDisagree;
            return outcome;
        }
    }

    // The whole node: one library call. Paired by position, which the fan-in's
    // authored order and the count check above are what make safe.
    std::vector<openstrata::motion::WeightedPose> weighted;
    weighted.reserve(inputs.poses.size());
    for (std::size_t i = 0; i < inputs.poses.size(); ++i)
    {
        weighted.push_back(
            openstrata::motion::WeightedPose{inputs.poses[i], inputs.weights[i]});
    }

    // Nothing weighted is the library's own answer now -- nullopt rather than a
    // default-constructed pose stamped at a 0.0 nobody sampled -- so this layer
    // reads it instead of looking for it first (usd-motion-plugins #6).
    std::optional<openstrata::motion::MotionPose> blended =
        openstrata::motion::BlendPoses(weighted);
    if (!blended)
    {
        outcome.refusal = BlendRefusal::NothingWeighted;
        return outcome;
    }
    outcome.pose = *blended;
    return outcome;
}

} // namespace execmotion
