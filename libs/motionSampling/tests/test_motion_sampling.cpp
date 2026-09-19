// SPDX-License-Identifier: Apache-2.0
#include "motionSampling/Blend.h"
#include "motionSampling/Filter.h"
#include "motionSampling/Interpolation.h"
#include "motionSampling/MotionSource.h"
#include "motionSampling/PoseBuffer.h"
#include "motionSampling/Resample.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <optional>

namespace
{

constexpr float kEpsilon = 1e-4f;

bool
NearlyEqual(float a, float b)
{
    return std::fabs(a - b) <= kEpsilon;
}

bool
NearlyEqual(const pxr::GfVec3f& a, const pxr::GfVec3f& b)
{
    return NearlyEqual(a[0], b[0]) && NearlyEqual(a[1], b[1]) && NearlyEqual(a[2], b[2]);
}

// Compares orientations, not representations: q and -q are the same rotation.
bool
SameOrientation(const pxr::GfQuatf& a, const pxr::GfQuatf& b)
{
    const pxr::GfQuatf na = a.GetNormalized();
    pxr::GfQuatf nb = b.GetNormalized();
    if (pxr::GfDot(na, nb) < 0.0f)
    {
        nb = pxr::GfQuatf(-nb.GetReal(), -nb.GetImaginary());
    }
    return NearlyEqual(na.GetReal(), nb.GetReal()) &&
           NearlyEqual(na.GetImaginary(), nb.GetImaginary());
}

pxr::GfQuatf
RotationX(float degrees)
{
    const float radians = degrees * 3.14159265358979324f / 180.0f;
    return pxr::GfQuatf(std::cos(radians * 0.5f),
                        pxr::GfVec3f(std::sin(radians * 0.5f), 0.0f, 0.0f));
}

openstrata::motion::MotionPose
MakePose(double timestamp, float hipsDegrees, const pxr::GfVec3f& rootPosition)
{
    openstrata::motion::MotionPose pose;
    pose.timestamp = timestamp;
    pose.localRotations[static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips)] = RotationX(hipsDegrees);
    pose.validRotations.set(static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips));
    pose.root.worldPosition = rootPosition;
    pose.root.hasPosition = true;
    return pose;
}

void
TestSlerpTakesTheShortArc()
{
    const pxr::GfQuatf identity(1.0f, pxr::GfVec3f(0.0f));
    const pxr::GfQuatf ninety = RotationX(90.0f);
    const pxr::GfQuatf ninetyFlipped(-ninety.GetReal(), -ninety.GetImaginary());

    const pxr::GfQuatf direct = openstrata::motion::SlerpShortest(identity, ninety, 0.5f);
    const pxr::GfQuatf viaFlipped = openstrata::motion::SlerpShortest(identity, ninetyFlipped, 0.5f);

    assert(SameOrientation(direct, RotationX(45.0f)));
    // The sign-flipped representative must not spin the long way round.
    assert(SameOrientation(direct, viaFlipped));

    assert(SameOrientation(openstrata::motion::SlerpShortest(identity, ninety, -1.0f), identity));
    assert(SameOrientation(openstrata::motion::SlerpShortest(identity, ninety, 2.0f), ninety));
}

void
TestMissingJointsAreHeldNotFaded()
{
    openstrata::motion::MotionPose a;
    a.timestamp = 0.0;
    const auto head = static_cast<std::size_t>(openstrata::motion::HumanJoint::Head);
    a.localRotations[head] = RotationX(40.0f);
    a.validRotations.set(head);

    openstrata::motion::MotionPose b;
    b.timestamp = 1.0;

    const openstrata::motion::MotionPose mid = openstrata::motion::LerpPose(a, b, 0.5f);
    assert(mid.validRotations.test(head));
    // Held at its only observed value rather than dragged toward identity.
    assert(SameOrientation(mid.localRotations[head], RotationX(40.0f)));
    assert(NearlyEqual(static_cast<float>(mid.timestamp), 0.5f));

    // A joint present in neither endpoint stays absent.
    const auto jaw = static_cast<std::size_t>(openstrata::motion::HumanJoint::Jaw);
    assert(!mid.validRotations.test(jaw));
}

void
TestChannelsAreHeldNotFaded()
{
    openstrata::motion::MotionPose a;
    a.timestamp = 0.0;
    a.channels.Set("happy", 0.2f);
    a.channels.Set("blink", 1.0f);

    openstrata::motion::MotionPose b;
    b.timestamp = 1.0;
    b.channels.Set("happy", 0.6f);
    b.channels.Set("aa", 0.5f);

    const openstrata::motion::MotionPose mid = openstrata::motion::LerpPose(a, b, 0.5f);

    // Reported by both: interpolated.
    const float* happy = mid.channels.Find("happy");
    assert(happy != nullptr && NearlyEqual(*happy, 0.4f));

    // Reported by one endpoint only: held at that weight. Fading it toward zero
    // would invent a blink closing that neither pose described -- the same rule
    // a missing joint follows, for the same reason.
    const float* blink = mid.channels.Find("blink");
    assert(blink != nullptr && NearlyEqual(*blink, 1.0f));
    const float* aa = mid.channels.Find("aa");
    assert(aa != nullptr && NearlyEqual(*aa, 0.5f));

    // A name neither endpoint reported stays unreported rather than becoming 0.
    assert(mid.channels.Find("sad") == nullptr);
    // The union arrives sorted, whichever endpoint each name came from.
    assert(mid.channels.entries.size() == 3);
    assert(mid.channels.entries[0].name == "aa");
    assert(mid.channels.entries[1].name == "blink");
    assert(mid.channels.entries[2].name == "happy");
}

void
TestLookAtTargetIsHeldNotFaded()
{
    openstrata::motion::MotionPose a;
    a.timestamp = 0.0;
    a.lookAtTarget = pxr::GfVec3f(0.0f, 1.5f, -2.0f);

    openstrata::motion::MotionPose b;
    b.timestamp = 1.0;
    b.lookAtTarget = pxr::GfVec3f(1.0f, 1.5f, -2.0f);

    // Reported by both: the target moves between them, which is the motion of
    // the thing being watched.
    const openstrata::motion::MotionPose mid = openstrata::motion::LerpPose(a, b, 0.5f);
    assert(mid.lookAtTarget);
    assert(NearlyEqual((*mid.lookAtTarget)[0], 0.5f));
    assert(NearlyEqual((*mid.lookAtTarget)[1], 1.5f));

    // Reported by one endpoint only: held. Easing it toward the origin would
    // aim the gaze at a place no producer named -- and the origin is a place,
    // which is exactly why it cannot double as "no target".
    openstrata::motion::MotionPose silent;
    silent.timestamp = 1.0;
    const openstrata::motion::MotionPose held = openstrata::motion::LerpPose(a, silent, 0.5f);
    assert(held.lookAtTarget && *held.lookAtTarget == *a.lookAtTarget);
    const openstrata::motion::MotionPose heldFromB = openstrata::motion::LerpPose(silent, a, 0.5f);
    assert(heldFromB.lookAtTarget && *heldFromB.lookAtTarget == *a.lookAtTarget);

    // Neither endpoint reported one: still none, rather than the origin.
    openstrata::motion::MotionPose alsoSilent;
    alsoSilent.timestamp = 1.0;
    assert(!openstrata::motion::LerpPose(silent, alsoSilent, 0.5f).lookAtTarget);
}

void
TestRootMotionFlagsSurviveInterpolation()
{
    openstrata::motion::RootMotion a;
    a.worldPosition = pxr::GfVec3f(0.0f, 1.0f, 0.0f);
    a.hasPosition = true;

    openstrata::motion::RootMotion b;
    b.worldPosition = pxr::GfVec3f(0.0f, 1.0f, 2.0f);
    b.hasPosition = true;
    b.worldOrientation = RotationX(90.0f);
    b.hasOrientation = true;

    const openstrata::motion::RootMotion mid = openstrata::motion::LerpRootMotion(a, b, 0.5f);
    assert(mid.hasPosition);
    assert(NearlyEqual(mid.worldPosition, pxr::GfVec3f(0.0f, 1.0f, 1.0f)));
    // Orientation is carried from the only endpoint that reported one.
    assert(mid.hasOrientation);
    assert(SameOrientation(mid.worldOrientation, RotationX(90.0f)));
    assert(!mid.hasLinearVelocity);
}

// Metadata is discrete and always present, so an interpolated pose takes the
// nearer observation's whole, its stamp and counter included -- never a stamp
// averaged into one no producer sent, and never the default because one side
// "had none".
void
TestMetadataSnapsToTheNearerEndpoint()
{
    using openstrata::motion::MotionPose;
    MotionPose a = MakePose(0.0, 0.0f, pxr::GfVec3f(0.0f));
    MotionPose b = MakePose(1.0, 10.0f, pxr::GfVec3f(1.0f));
    a.metadata.provider = "example.sender";
    a.metadata.sourceTimestamp = 100.0;
    a.metadata.sequenceNumber = 41;
    b.metadata.provider = "example.sender";
    b.metadata.sourceTimestamp = 100.5;
    b.metadata.sequenceNumber = 42;

    const MotionPose early = openstrata::motion::LerpPose(a, b, 0.25f);
    assert(early.metadata == a.metadata);
    const MotionPose late = openstrata::motion::LerpPose(a, b, 0.75f);
    assert(late.metadata == b.metadata);

    // A default metadata is a value, not an absence: the nearer side's default
    // wins over the farther side's stamp.
    MotionPose unrecorded = a;
    unrecorded.metadata = openstrata::motion::SourceMetadata{};
    const MotionPose held = openstrata::motion::LerpPose(unrecorded, b, 0.25f);
    assert(held.metadata == openstrata::motion::SourceMetadata{});
}

void
TestPoseBufferOrderingAndSampling()
{
    openstrata::motion::PoseBuffer buffer(3);
    assert(buffer.IsEmpty());
    assert(!buffer.Sample(0.0).has_value());

    assert(buffer.Push(MakePose(0.0, 0.0f, pxr::GfVec3f(0.0f))));
    assert(buffer.Push(MakePose(1.0, 90.0f, pxr::GfVec3f(0.0f, 0.0f, 1.0f))));
    // Out-of-order and duplicate timestamps are refused, not reordered.
    assert(!buffer.Push(MakePose(0.5, 0.0f, pxr::GfVec3f(0.0f))));
    assert(!buffer.Push(MakePose(1.0, 0.0f, pxr::GfVec3f(0.0f))));
    assert(buffer.GetSize() == 2);

    double start = 0.0;
    double end = 0.0;
    assert(buffer.GetTimeRange(&start, &end));
    assert(NearlyEqual(static_cast<float>(start), 0.0f));
    assert(NearlyEqual(static_cast<float>(end), 1.0f));

    const auto mid = buffer.Sample(0.5);
    assert(mid.has_value());
    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);
    assert(SameOrientation(mid->localRotations[hips], RotationX(45.0f)));
    assert(NearlyEqual(mid->root.worldPosition, pxr::GfVec3f(0.0f, 0.0f, 0.5f)));

    // Outside the buffered range the boundary pose is held.
    assert(SameOrientation(buffer.Sample(-5.0)->localRotations[hips], RotationX(0.0f)));
    assert(SameOrientation(buffer.Sample(5.0)->localRotations[hips], RotationX(90.0f)));

    // Capacity evicts oldest-first.
    assert(buffer.Push(MakePose(2.0, 90.0f, pxr::GfVec3f(0.0f, 0.0f, 2.0f))));
    assert(buffer.Push(MakePose(3.0, 90.0f, pxr::GfVec3f(0.0f, 0.0f, 3.0f))));
    assert(buffer.GetSize() == 3);
    assert(NearlyEqual(static_cast<float>(buffer.GetOldest().timestamp), 1.0f));
    assert(NearlyEqual(static_cast<float>(buffer.GetNewest().timestamp), 3.0f));
}

void
TestPoseBufferExtrapolatesPositionOnly()
{
    openstrata::motion::PoseBuffer buffer;
    buffer.Push(MakePose(0.0, 0.0f, pxr::GfVec3f(0.0f)));
    buffer.Push(MakePose(1.0, 90.0f, pxr::GfVec3f(0.0f, 0.0f, 1.0f)));

    const auto lead = buffer.SampleExtrapolated(1.1, 0.2);
    assert(lead.has_value());
    // 1 m/s derived from the last two samples, advanced by 0.1 s.
    assert(NearlyEqual(lead->root.worldPosition, pxr::GfVec3f(0.0f, 0.0f, 1.1f)));
    // Rotation is held, never extrapolated.
    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);
    assert(SameOrientation(lead->localRotations[hips], RotationX(90.0f)));

    // The lead is capped.
    const auto capped = buffer.SampleExtrapolated(5.0, 0.2);
    assert(NearlyEqual(capped->root.worldPosition, pxr::GfVec3f(0.0f, 0.0f, 1.2f)));

    // Before the newest sample it behaves exactly like Sample.
    const auto inside = buffer.SampleExtrapolated(0.5, 0.2);
    assert(SameOrientation(inside->localRotations[hips], RotationX(45.0f)));
}

void
TestResampleCoversTheWholeInterval()
{
    openstrata::motion::MotionClip animation;
    animation.startTime = 0.0;
    animation.endTime = 1.0;
    animation.nominalFrameRate = 30.0;
    animation.samples.push_back(MakePose(0.0, 0.0f, pxr::GfVec3f(0.0f)));
    animation.samples.push_back(MakePose(1.0, 90.0f, pxr::GfVec3f(0.0f, 0.0f, 1.0f)));

    const openstrata::motion::MotionClip uniform = openstrata::motion::Resample(animation, 4.0);
    assert(uniform.samples.size() == 5);
    assert(NearlyEqual(static_cast<float>(uniform.nominalFrameRate), 4.0f));
    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);
    assert(SameOrientation(uniform.samples[1].localRotations[hips], RotationX(22.5f)));
    assert(NearlyEqual(static_cast<float>(uniform.samples.back().timestamp), 1.0f));

    // A duration that is not a multiple of the step still ends on endTime.
    animation.endTime = 0.7;
    const openstrata::motion::MotionClip ragged = openstrata::motion::Resample(animation, 4.0);
    assert(NearlyEqual(static_cast<float>(ragged.samples.back().timestamp), 0.7f));

    // A clip that carries samples but never declared its interval falls back to
    // the sample timestamps instead of collapsing to a single pose.
    openstrata::motion::MotionClip undeclared;
    undeclared.samples = animation.samples;
    const openstrata::motion::MotionClip recovered = openstrata::motion::Resample(undeclared, 4.0);
    assert(recovered.samples.size() == 5);
    assert(NearlyEqual(static_cast<float>(recovered.endTime), 1.0f));

    // Degenerate inputs yield no samples rather than a fabricated timeline.
    assert(openstrata::motion::Resample(animation, 0.0).samples.empty());
    assert(openstrata::motion::Resample(openstrata::motion::MotionClip(), 30.0).samples.empty());
}

void
TestFilterIsFrameRateIndependentAndTolerantOfDropouts()
{
    openstrata::motion::PoseFilter::Options options;
    options.cutoffHz = 0.0f;
    openstrata::motion::PoseFilter passthrough(options);
    const openstrata::motion::MotionPose raw = MakePose(0.0, 90.0f, pxr::GfVec3f(0.0f));
    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);
    assert(SameOrientation(passthrough.Apply(raw).localRotations[hips], RotationX(90.0f)));

    options.cutoffHz = 1.0f;
    openstrata::motion::PoseFilter filter(options);
    // The first pose seeds the state and passes through untouched.
    assert(
        SameOrientation(filter.Apply(MakePose(0.0, 0.0f, pxr::GfVec3f(0.0f))).localRotations[hips],
                        RotationX(0.0f)));

    const openstrata::motion::MotionPose smoothed =
        filter.Apply(MakePose(0.1, 90.0f, pxr::GfVec3f(0.0f, 0.0f, 1.0f)));
    // Moves toward the new sample without reaching it.
    const float real = smoothed.localRotations[hips].GetNormalized().GetReal();
    assert(real < std::cos(0.0f));
    assert(real > RotationX(90.0f).GetReal());
    assert(smoothed.root.worldPosition[2] > 0.0f);
    assert(smoothed.root.worldPosition[2] < 1.0f);

    // A joint that drops out is not invented, and its history is retained.
    openstrata::motion::MotionPose withoutHips;
    withoutHips.timestamp = 0.2;
    const openstrata::motion::MotionPose dropout = filter.Apply(withoutHips);
    assert(!dropout.validRotations.test(hips));
    assert(filter.HasState());

    filter.Reset();
    assert(!filter.HasState());
}

void
TestBlendWeightsAndUnitLength()
{
    const openstrata::motion::MotionPose a = MakePose(0.0, 0.0f, pxr::GfVec3f(0.0f));
    const openstrata::motion::MotionPose b = MakePose(0.0, 90.0f, pxr::GfVec3f(0.0f, 0.0f, 1.0f));
    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);

    assert(SameOrientation(openstrata::motion::BlendPoses(a, b, 0.0f).localRotations[hips], RotationX(0.0f)));
    assert(SameOrientation(openstrata::motion::BlendPoses(a, b, 1.0f).localRotations[hips], RotationX(90.0f)));

    const std::optional<openstrata::motion::MotionPose> even =
        openstrata::motion::BlendPoses({{a, 1.0f}, {b, 1.0f}});
    assert(even);
    assert(SameOrientation(even->localRotations[hips], RotationX(45.0f)));
    assert(NearlyEqual(even->localRotations[hips].GetLength(), 1.0f));

    // Non-positive weights drop out; the surviving pose wins outright.
    const std::optional<openstrata::motion::MotionPose> skewed =
        openstrata::motion::BlendPoses({{a, 0.0f}, {b, 2.0f}, {a, -1.0f}});
    assert(skewed);
    assert(SameOrientation(skewed->localRotations[hips], RotationX(90.0f)));
}

// Nothing weighted is an answer of its own, not a default pose stamped 0.0.
void
TestBlendCanSayThereIsNothingToBlend()
{
    const openstrata::motion::MotionPose a = MakePose(2.0, 0.0f, pxr::GfVec3f(0.0f));
    const openstrata::motion::MotionPose b = MakePose(2.0, 90.0f, pxr::GfVec3f(0.0f));

    assert(!openstrata::motion::BlendPoses({}));
    assert(!openstrata::motion::BlendPoses({{a, 0.0f}, {b, -1.0f}}));
    // A NaN is no weight, like a negative one, rather than a NaN rotation.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    assert(!openstrata::motion::BlendPoses({{a, nan}}));
    const std::optional<openstrata::motion::MotionPose> besideNan =
        openstrata::motion::BlendPoses({{a, nan}, {b, 1.0f}});
    assert(besideNan);
    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);
    assert(SameOrientation(besideNan->localRotations[hips], RotationX(90.0f)));
}

// Sources combined at one instant are stamped at it, never at a time between
// two of them: a violated precondition answers the first weighted source's
// instant, which is at least one somebody sampled.
void
TestBlendIsStampedAtTheSourcesInstant()
{
    const openstrata::motion::MotionPose a = MakePose(1.0, 0.0f, pxr::GfVec3f(0.0f));
    const openstrata::motion::MotionPose b = MakePose(1.0, 90.0f, pxr::GfVec3f(0.0f));
    const openstrata::motion::MotionPose c = MakePose(1.0, 45.0f, pxr::GfVec3f(0.0f));
    assert(openstrata::motion::BlendPoses({{a, 1.0f}, {b, 1.0f}, {c, 1.0f}})->timestamp == 1.0);

    // The measured case: clips at 1.0 s and 0.5 s used to be stamped 0.625 s.
    const openstrata::motion::MotionPose early = MakePose(0.5, 90.0f, pxr::GfVec3f(0.0f));
    assert(openstrata::motion::BlendPoses({{a, 3.0f}, {early, 1.0f}})->timestamp == 1.0);
    assert(openstrata::motion::BlendPoses({{a, 0.0f}, {early, 1.0f}, {a, 1.0f}})->timestamp == 0.5);
}

// The status-carrying answer, from a clip held by reference.
void
TestSampleClipCarriesTheStatus()
{
    using openstrata::motion::PoseSampleStatus;
    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);

    const openstrata::motion::PoseSampleResult none =
        openstrata::motion::SampleClip(openstrata::motion::MotionClip(), 0.0);
    assert(none.status == PoseSampleStatus::Unavailable);
    assert(!none.pose);
    assert(none.lag == 0.0);

    openstrata::motion::MotionClip clip;
    clip.samples.push_back(MakePose(0.0, 0.0f, pxr::GfVec3f(0.0f)));
    clip.samples.push_back(MakePose(1.0, 90.0f, pxr::GfVec3f(0.0f)));

    const openstrata::motion::PoseSampleResult inside = openstrata::motion::SampleClip(clip, 0.5);
    assert(inside.status == PoseSampleStatus::Sampled);
    assert(SameOrientation(inside.pose->localRotations[hips], RotationX(45.0f)));
    assert(inside.pose->timestamp == 0.5);
    assert(inside.lag == -0.5);

    // On a boundary to within the tolerance is a sample, not a hold.
    assert(openstrata::motion::SampleClip(clip, 1.0 + 1e-7).status == PoseSampleStatus::Sampled);

    const openstrata::motion::PoseSampleResult past = openstrata::motion::SampleClip(clip, 3.0);
    assert(past.status == PoseSampleStatus::Held);
    assert(SameOrientation(past.pose->localRotations[hips], RotationX(90.0f)));
    assert(past.pose->timestamp == 3.0);
    assert(past.lag == 2.0);
    assert(openstrata::motion::SampleClip(clip, -1.0).status == PoseSampleStatus::Held);

    // One search: the source, the pose-only function and a buffer holding the
    // same samples answer the same pose.
    openstrata::motion::ClipSource source(clip);
    openstrata::motion::PoseBuffer buffer;
    assert(buffer.Push(clip.samples[0]) && buffer.Push(clip.samples[1]));
    for (const double t : {-1.0, 0.0, 0.25, 0.5, 1.0, 3.0})
    {
        const openstrata::motion::PoseSampleResult direct = openstrata::motion::SampleClip(clip, t);
        assert(source.Sample(t) == direct);
        openstrata::motion::MotionPose pose = openstrata::motion::SampleAnimation(clip, t);
        pose.timestamp = t;
        assert(pose == *direct.pose);
        std::optional<openstrata::motion::MotionPose> buffered = buffer.Sample(t);
        buffered->timestamp = t;
        assert(*buffered == *direct.pose);
    }

    // A source's offset is the same answer on the consumer's clock.
    source.SetStartOffset(10.0);
    openstrata::motion::PoseSampleResult shifted = source.Sample(10.5);
    assert(shifted.pose->timestamp == 10.5);
    shifted.pose->timestamp = 0.5;
    assert(shifted == inside);
}

// The streaming filter as a pure function: carrying the step's state
// reproduces the stream exactly, and carrying only its pose is the measured
// dropout loss.
void
TestFilterStepCarriesTheStateTheStreamKeeps()
{
    using openstrata::motion::PoseFilter;
    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);
    PoseFilter::Options options;
    options.cutoffHz = 1.0f;

    openstrata::motion::MotionPose missing;
    missing.timestamp = 0.1;
    const openstrata::motion::MotionPose frames[] = {
        MakePose(0.0, 0.0f, pxr::GfVec3f(0.0f)),
        missing,
        MakePose(0.2, 90.0f, pxr::GfVec3f(0.0f, 0.0f, 1.0f)),
    };

    PoseFilter streamed(options);
    std::optional<openstrata::motion::MotionPose> state;
    openstrata::motion::MotionPose lastStreamed;
    openstrata::motion::MotionPose lastStepped;
    for (const openstrata::motion::MotionPose& frame : frames)
    {
        lastStreamed = streamed.Apply(frame);
        PoseFilter::StepResult step = PoseFilter::Step(state ? &*state : nullptr, frame, options);
        lastStepped = step.pose;
        state = std::move(step.state);
    }
    assert(lastStepped == lastStreamed);
    // The hips came back smoothed against the frame before the dropout.
    assert(!SameOrientation(lastStepped.localRotations[hips], RotationX(90.0f)));

    // The state is richer than the pose: the dropped joint lives only there.
    const PoseFilter::StepResult seeded = PoseFilter::Step(nullptr, frames[0], options);
    assert(seeded.pose == frames[0] && seeded.state == frames[0]);
    const PoseFilter::StepResult dropped = PoseFilter::Step(&seeded.state, frames[1], options);
    assert(!dropped.pose.validRotations.test(hips));
    assert(dropped.state.validRotations.test(hips));

    // Carrying the pose instead of the state loses the history: the joint
    // passes through unsmoothed.
    const PoseFilter::StepResult poseCarried = PoseFilter::Step(&dropped.pose, frames[2], options);
    assert(SameOrientation(poseCarried.pose.localRotations[hips], RotationX(90.0f)));

    // A pose not later than the state reseeds, as the stream does.
    const PoseFilter::StepResult reseeded = PoseFilter::Step(&frames[2], frames[0], options);
    assert(reseeded.pose == frames[0] && reseeded.state == frames[0]);
}

} // namespace

int
main()
{
    TestSlerpTakesTheShortArc();
    TestMissingJointsAreHeldNotFaded();
    TestChannelsAreHeldNotFaded();
    TestLookAtTargetIsHeldNotFaded();
    TestRootMotionFlagsSurviveInterpolation();
    TestMetadataSnapsToTheNearerEndpoint();
    TestPoseBufferOrderingAndSampling();
    TestPoseBufferExtrapolatesPositionOnly();
    TestResampleCoversTheWholeInterval();
    TestFilterIsFrameRateIndependentAndTolerantOfDropouts();
    TestBlendWeightsAndUnitLength();
    TestBlendCanSayThereIsNothingToBlend();
    TestBlendIsStampedAtTheSourcesInstant();
    TestSampleClipCarriesTheStatus();
    TestFilterStepCarriesTheStateTheStreamKeeps();
    std::puts("motionSampling unit tests passed");
    return 0;
}
