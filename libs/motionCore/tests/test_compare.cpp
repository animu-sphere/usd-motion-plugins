// SPDX-License-Identifier: Apache-2.0
//
// The two comparisons, and the decisions Compare.h states about them.
#include "motionCore/Compare.h"
#include "motionCore/MotionPose.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

namespace
{

using openstrata::motion::HumanJoint;
using openstrata::motion::HumanJointCount;
using openstrata::motion::MotionClip;
using openstrata::motion::MotionPose;
using openstrata::motion::SourceMetadata;
using openstrata::motion::MotionTolerance;
using openstrata::motion::NearlyEqual;

constexpr double kPi = 3.14159265358979323846;

pxr::GfQuatf
AboutY(double radians)
{
    return pxr::GfQuatf(static_cast<float>(std::cos(radians * 0.5)),
                        pxr::GfVec3f(0.0f, static_cast<float>(std::sin(radians * 0.5)), 0.0f));
}

pxr::GfQuatf
Negated(const pxr::GfQuatf& q)
{
    return pxr::GfQuatf(-q.GetReal(), -q.GetImaginary());
}

// Six decimals, the way the recorded-trace writer rounds. Compare.h derives
// every default tolerance from surviving exactly this.
float
Rounded(float value)
{
    return static_cast<float>(std::round(static_cast<double>(value) * 1e6) / 1e6);
}

pxr::GfQuatf
Rounded(const pxr::GfQuatf& q)
{
    return pxr::GfQuatf(Rounded(q.GetReal()),
                        pxr::GfVec3f(Rounded(q.GetImaginary()[0]), Rounded(q.GetImaginary()[1]),
                                     Rounded(q.GetImaginary()[2])));
}

// A pose with enough of every field set that a comparison has something to
// walk: two joints, a root carrying position and linear velocity, confidence,
// contacts, and provenance.
MotionPose
SamplePose()
{
    MotionPose pose;
    pose.timestamp = 1.5;
    pose.root.worldPosition = pxr::GfVec3f(0.25f, 0.9f, -1.0f);
    pose.root.hasPosition = true;
    pose.root.linearVelocity = pxr::GfVec3f(0.0f, 0.0f, 1.2f);
    pose.root.hasLinearVelocity = true;

    pose.localRotations[static_cast<std::size_t>(HumanJoint::Hips)] = AboutY(0.1);
    pose.validRotations.set(static_cast<std::size_t>(HumanJoint::Hips));
    pose.localRotations[static_cast<std::size_t>(HumanJoint::LeftUpperArm)] = AboutY(-0.75);
    pose.validRotations.set(static_cast<std::size_t>(HumanJoint::LeftUpperArm));

    std::array<float, HumanJointCount> confidence{};
    confidence.fill(1.0f);
    confidence[static_cast<std::size_t>(HumanJoint::LeftUpperArm)] = 0.5f;
    pose.confidence = confidence;

    openstrata::motion::ContactState contacts;
    contacts.leftFoot = openstrata::motion::FootContact::InContact;
    contacts.rightFoot = openstrata::motion::FootContact::NotInContact;
    pose.contacts = contacts;

    SourceMetadata source;
    source.kind = openstrata::motion::MotionSourceKind::LiveCapture;
    source.provider = "example.sender";
    source.protocol = "vmc";
    source.sourceId = "session-01";
    pose.source = source;
    return pose;
}

MotionClip
SampleAnimation()
{
    MotionClip animation;
    for (int frame = 0; frame != 3; ++frame)
    {
        MotionPose pose = SamplePose();
        pose.timestamp = frame / 30.0;
        pose.localRotations[static_cast<std::size_t>(HumanJoint::Hips)] = AboutY(0.1 * frame);
        animation.samples.push_back(pose);
    }
    animation.startTime = 0.0;
    animation.endTime = 2.0 / 30.0;
    animation.nominalFrameRate = 30.0;
    animation.source = *animation.samples.front().source;
    return animation;
}

void
TestAngleBetween()
{
    const pxr::GfQuatf identity(1.0f, pxr::GfVec3f(0.0f));
    assert(openstrata::motion::AngleBetween(identity, identity) == 0.0f);

    // The double cover: the same orientation, the opposite components.
    const pxr::GfQuatf quarter = AboutY(kPi * 0.5);
    assert(std::abs(openstrata::motion::AngleBetween(identity, quarter) - kPi * 0.5) < 1e-5);

    // Antipodal orientations are pi apart, not 2pi: the arc is the short one.
    assert(std::abs(openstrata::motion::AngleBetween(identity, AboutY(kPi)) - kPi) < 1e-5);

    // A zero quaternion is not an orientation, so it is no distance from
    // anything -- including itself.
    const pxr::GfQuatf zero(0.0f, pxr::GfVec3f(0.0f));
    assert(std::isnan(openstrata::motion::AngleBetween(zero, identity)));
    assert(std::isnan(openstrata::motion::AngleBetween(zero, zero)));

    // The numerics, which are not incidental here. `acos` is infinitely steep
    // at 1, so a relative mismatch of e between the dot product and the length
    // product surfaces as roughly 2*sqrt(2e) radians -- 9e-4 for a float-sized
    // e, which is an order above the default tolerance and lands exactly where
    // the answer should be zero. Both are formed in double from the same
    // components, so these hold on every platform rather than on the one the
    // rounding happened to suit.
    const pxr::GfQuatf unnormalised = AboutY(0.4) * 7.5f;
    assert(openstrata::motion::AngleBetween(unnormalised, unnormalised) == 0.0f);
    assert(openstrata::motion::AngleBetween(quarter, quarter) == 0.0f);
    assert(openstrata::motion::AngleBetween(quarter, Negated(quarter)) == 0.0f);

    // Length is not orientation: scaling one side changes nothing measurable.
    // Not exactly zero -- scaling rounds each component -- but three orders
    // below the default tolerance rather than one above it.
    assert(openstrata::motion::AngleBetween(AboutY(0.4), unnormalised) < 1e-5f);
}

void
TestExactEquality()
{
    const MotionPose pose = SamplePose();
    assert(pose == pose);
    assert(!(pose != pose));

    MotionPose other = pose;
    other.localRotations[static_cast<std::size_t>(HumanJoint::Hips)] = AboutY(0.100001);
    assert(other != pose);

    // A default-constructed pose claims no joints, so two of them are equal
    // whatever their rotation slots hold.
    MotionPose emptyA;
    MotionPose emptyB;
    emptyB.localRotations[static_cast<std::size_t>(HumanJoint::Head)] = AboutY(1.0);
    assert(emptyA == emptyB);
    assert(NearlyEqual(emptyA, emptyB));

    // The claim itself is compared: one pose carrying a joint the other does
    // not is a different pose even when the rotations match.
    MotionPose withHead = emptyA;
    withHead.validRotations.set(static_cast<std::size_t>(HumanJoint::Head));
    assert(withHead != emptyA);
    assert(!NearlyEqual(withHead, emptyA));

    // The same rule on the root: an unset field's value is not part of the
    // pose.
    MotionPose rootA;
    MotionPose rootB;
    rootB.root.worldPosition = pxr::GfVec3f(5.0f, 5.0f, 5.0f);
    assert(rootA == rootB);
    rootB.root.hasPosition = true;
    assert(rootA != rootB);

    // Confidence is read only where a joint is claimed, and its presence is a
    // fact of its own: an adapter that cannot measure confidence reports none.
    MotionPose loose = pose;
    (*loose.confidence)[static_cast<std::size_t>(HumanJoint::Head)] = 0.0f;
    assert(loose == pose);
    loose.confidence.reset();
    assert(loose != pose);
    assert(!NearlyEqual(loose, pose));
}

void
TestValueAndMotionDiverge()
{
    const MotionPose pose = SamplePose();

    // A quaternion and its negation: the same motion, a different value.
    MotionPose flipped = pose;
    const std::size_t arm = static_cast<std::size_t>(HumanJoint::LeftUpperArm);
    flipped.localRotations[arm] = Negated(pose.localRotations[arm]);
    assert(flipped != pose);
    assert(NearlyEqual(flipped, pose));

    // Provenance: a different value, the same motion.
    MotionPose relabelled = pose;
    relabelled.source->provider = "other.sender";
    assert(relabelled != pose);
    assert(NearlyEqual(relabelled, pose));

    MotionPose anonymous = pose;
    anonymous.source.reset();
    assert(anonymous != pose);
    assert(NearlyEqual(anonymous, pose));

    // And on an animation, where the metadata is not optional.
    MotionClip clip = SampleAnimation();
    MotionClip renamed = clip;
    renamed.source.sourceId = "session-02";
    assert(renamed != clip);
    assert(NearlyEqual(renamed, clip));
}

void
TestTolerance()
{
    const MotionPose pose = SamplePose();
    const MotionTolerance tolerance;

    // A pose written through the trace format's six decimals and read back is
    // the same motion. This is the floor every default is derived from.
    MotionPose quantised = pose;
    for (std::size_t index = 0; index != HumanJointCount; ++index)
    {
        quantised.localRotations[index] = Rounded(pose.localRotations[index]);
    }
    quantised.root.worldPosition =
        pxr::GfVec3f(Rounded(pose.root.worldPosition[0]), Rounded(pose.root.worldPosition[1]),
                     Rounded(pose.root.worldPosition[2]));
    quantised.timestamp = std::round(pose.timestamp * 1e6) / 1e6;
    assert(NearlyEqual(quantised, pose));

    // Each tolerance holds either side of its own limit.
    MotionPose nudged = pose;
    const std::size_t hips = static_cast<std::size_t>(HumanJoint::Hips);
    nudged.localRotations[hips] = AboutY(0.1 + 1e-5);
    assert(NearlyEqual(nudged, pose));
    nudged.localRotations[hips] = AboutY(0.1 + 1e-3);
    assert(!NearlyEqual(nudged, pose));

    MotionPose moved = pose;
    moved.root.worldPosition[1] += 1e-6f;
    assert(NearlyEqual(moved, pose));
    moved.root.worldPosition[1] += 1e-3f;
    assert(!NearlyEqual(moved, pose));

    MotionPose later = pose;
    later.timestamp += 1e-7;
    assert(NearlyEqual(later, pose));
    later.timestamp += 1e-3;
    assert(!NearlyEqual(later, pose));

    // A velocity is allowed more room than a position, because it is derived
    // by dividing one by a frame interval.
    MotionPose drifting = pose;
    drifting.root.linearVelocity[2] += 5e-5f;
    assert(NearlyEqual(drifting, pose));
    assert(!NearlyEqual(drifting, pose, MotionTolerance{1e-4f, 1e-5f, 1e-6f}));

    // The caller can state its own, and the stated one is what is applied.
    assert(NearlyEqual(nudged, pose, MotionTolerance{1e-2f}));
}

void
TestNonFinite()
{
    MotionPose pose = SamplePose();
    pose.timestamp = std::numeric_limits<double>::quiet_NaN();
    // A NaN equals nothing, including itself, under both comparisons.
    assert(pose != pose);
    assert(!NearlyEqual(pose, pose));

    MotionPose broken = SamplePose();
    broken.localRotations[static_cast<std::size_t>(HumanJoint::Hips)] =
        pxr::GfQuatf(std::numeric_limits<float>::quiet_NaN(), pxr::GfVec3f(0.0f));
    assert(broken != broken);
    assert(!NearlyEqual(broken, broken));
}

void
TestDifferenceReport()
{
    const MotionPose pose = SamplePose();
    std::string difference = "untouched";

    // Nothing is written when the two agree.
    assert(NearlyEqual(pose, pose, MotionTolerance{}, &difference));
    assert(difference == "untouched");

    MotionPose nudged = pose;
    nudged.localRotations[static_cast<std::size_t>(HumanJoint::LeftUpperArm)] = AboutY(-0.75 + 0.01);
    assert(!NearlyEqual(nudged, pose, MotionTolerance{}, &difference));
    assert(difference.rfind("leftUpperArm rotation differs by ", 0) == 0);

    // The first difference in a fixed order, so the same pair always reports
    // the same line: the timestamp is checked before any joint.
    nudged.timestamp += 1.0;
    assert(!NearlyEqual(nudged, pose, MotionTolerance{}, &difference));
    assert(difference.rfind("timestamp differs by ", 0) == 0);

    MotionPose absent = pose;
    absent.validRotations.reset(static_cast<std::size_t>(HumanJoint::LeftUpperArm));
    assert(!NearlyEqual(absent, pose, MotionTolerance{}, &difference));
    assert(difference == "leftUpperArm is present only in the second pose");
    assert(!NearlyEqual(pose, absent, MotionTolerance{}, &difference));
    assert(difference == "leftUpperArm is present only in the first pose");
}

void
TestAnimation()
{
    const MotionClip clip = SampleAnimation();
    assert(clip == clip);
    assert(NearlyEqual(clip, clip));

    std::string difference;
    MotionClip shorter = clip;
    shorter.samples.pop_back();
    assert(shorter != clip);
    assert(!NearlyEqual(shorter, clip, MotionTolerance{}, &difference));
    assert(difference == "sample count differs: 2 vs 3");

    // A sample-level difference is reported with the sample that carried it.
    MotionClip bent = clip;
    bent.samples[1].localRotations[static_cast<std::size_t>(HumanJoint::Hips)] = AboutY(0.5);
    assert(bent != clip);
    assert(!NearlyEqual(bent, clip, MotionTolerance{}, &difference));
    assert(difference.rfind("sample 1: hips rotation differs by ", 0) == 0);

    MotionClip faster = clip;
    faster.nominalFrameRate = 60.0;
    assert(faster != clip);
    assert(!NearlyEqual(faster, clip, MotionTolerance{}, &difference));
    assert(difference.rfind("nominalFrameRate differs by ", 0) == 0);
}

void
TestChannels()
{
    MotionPose reported;
    reported.channels.Set("happy", 0.5f);
    reported.channels.Set("aa", 0.25f);

    // The set is sorted, so the order two producers reported the same weights
    // in cannot make them different values.
    MotionPose reversed;
    reversed.channels.Set("aa", 0.25f);
    reversed.channels.Set("happy", 0.5f);
    assert(reversed == reported && NearlyEqual(reversed, reported));

    // An unreported name is not a zero weight, under either comparison. This is
    // the pose-level half of the rule `Find` states at the value level.
    MotionPose zeroed = reported;
    zeroed.channels.Set("blink", 0.0f);
    std::string difference;
    assert(zeroed != reported);
    assert(!NearlyEqual(zeroed, reported, MotionTolerance{}, &difference));
    assert(difference == "channel 'blink' is reported only by the first pose");

    // One set a strict prefix of the other. This reaches the comparison by a
    // different route than the case above -- every shared index agrees and only
    // the counts differ -- and it is the shape a producer that stopped
    // reporting its last channel actually takes.
    MotionPose extra = reported;
    extra.channels.Set("zz", 0.1f);
    assert(extra != reported);
    assert(!NearlyEqual(extra, reported, MotionTolerance{}, &difference));
    assert(difference == "channel 'zz' is reported only by the first pose");
    assert(!NearlyEqual(reported, extra, MotionTolerance{}, &difference));
    assert(difference == "channel 'zz' is reported only by the second pose");

    // A name is an identifier: no tolerance makes two spellings one channel.
    MotionPose misspelt;
    misspelt.channels.Set("happy", 0.5f);
    misspelt.channels.Set("aa", 0.25f);
    misspelt.channels.entries[1].name = "happyy";
    assert(misspelt != reported);
    assert(!NearlyEqual(misspelt, reported, MotionTolerance{}, &difference));
    assert(difference == "channel 'happy' is reported only by the second pose");

    // A weight does take one, and it has to be wide enough for the trace
    // format: these two are adjacent six-decimal values, so a fixture and the
    // pose that produced it land here routinely. Different values, same motion.
    MotionPose quantum = reported;
    quantum.channels.Set("happy", Rounded(0.500001f));
    assert(quantum != reported);
    assert(NearlyEqual(quantum, reported));

    MotionPose wider = reported;
    wider.channels.Set("happy", 0.5f + 1e-3f);
    assert(!NearlyEqual(wider, reported, MotionTolerance{}, &difference));
    assert(difference.rfind("channel 'happy' value differs by ", 0) == 0);
}

void
TestLookAtTarget()
{
    MotionPose watching;
    watching.lookAtTarget = pxr::GfVec3f(0.0f, 1.5f, -2.0f);

    MotionPose same = watching;
    assert(same == watching && NearlyEqual(same, watching));

    // Reporting no target is not looking at the origin. A pose that said
    // nothing about the gaze and one that aimed it at (0, 0, 0) are different
    // claims, so neither comparison may call them equal -- and the origin is
    // the one point where a sentinel would have made them the same value.
    MotionPose silent;
    MotionPose origin;
    origin.lookAtTarget = pxr::GfVec3f(0.0f);
    std::string difference;
    assert(origin != silent);
    assert(!NearlyEqual(origin, silent, MotionTolerance{}, &difference));
    assert(difference == "only one pose reports a look-at target");
    assert(!NearlyEqual(silent, origin, MotionTolerance{}, &difference));
    assert(difference == "only one pose reports a look-at target");

    // A target is a point, so it takes the distance tolerance a root position
    // does: six-decimal rounding is the same motion, a centimetre is not.
    MotionPose quantum = watching;
    quantum.lookAtTarget = pxr::GfVec3f(0.0f, Rounded(1.500001f), -2.0f);
    assert(quantum != watching);
    assert(NearlyEqual(quantum, watching));

    MotionPose elsewhere = watching;
    elsewhere.lookAtTarget = pxr::GfVec3f(0.0f, 1.51f, -2.0f);
    assert(!NearlyEqual(elsewhere, watching, MotionTolerance{}, &difference));
    assert(difference.rfind("look-at target differs by ", 0) == 0);
}

void
TestExactImpliesNearly()
{
    // The two comparisons read the same fields, so the strict one can never
    // accept a pair the tolerant one rejects.
    const MotionPose pose = SamplePose();
    MotionPose copy = pose;
    assert(copy == pose && NearlyEqual(copy, pose));

    const MotionClip clip = SampleAnimation();
    MotionClip clipCopy = clip;
    assert(clipCopy == clip && NearlyEqual(clipCopy, clip));

    MotionPose empty;
    assert(empty == MotionPose() && NearlyEqual(empty, MotionPose()));
}

} // namespace

int
main()
{
    TestAngleBetween();
    TestExactEquality();
    TestValueAndMotionDiverge();
    TestTolerance();
    TestNonFinite();
    TestDifferenceReport();
    TestAnimation();
    TestChannels();
    TestLookAtTarget();
    TestExactImpliesNearly();
    return 0;
}
