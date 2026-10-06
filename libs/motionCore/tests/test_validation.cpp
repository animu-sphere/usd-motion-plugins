// SPDX-License-Identifier: Apache-2.0
#include "motionCore/Validation.h"

#include <cassert>
#include <limits>
#include <set>

using namespace openstrata::motion;

bool
Has(const ValidationReport& report, ValidationCode code, std::string_view subject)
{
    for (const auto& d : report.reported)
        if (d.code == code && d.subject == subject)
            return true;
    return false;
}

int
main()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const auto hips = static_cast<std::size_t>(HumanJoint::Hips);
    MotionPose pose;
    // Poison absent payloads: missing is not a reported zero or identity.
    pose.localRotations[hips] = pxr::GfQuatf(nan);
    pose.root.worldPosition = pxr::GfVec3f(inf);
    pose.root.worldOrientation = pxr::GfQuatf(0.0f);
    assert(ValidateMotionPose(pose).IsValid());
    pose.validRotations.set(hips);
    auto report = ValidateMotionPose(pose);
    assert(Has(report, ValidationCode::InvalidQuaternion, "localRotations.hips"));
    pose.localRotations[hips] = pxr::GfQuatf(2.0f);
    assert(Has(ValidateMotionPose(pose), ValidationCode::NonUnitQuaternion, "localRotations.hips"));
    assert(ValidateMotionPose(pose, QuaternionValidationPolicy::Normalizable).IsValid());
    assert(pose.localRotations[hips].GetReal() == 2.0f);
    pose.localRotations[hips] = pxr::GfQuatf(0.0f);
    assert(!ValidateMotionPose(pose, QuaternionValidationPolicy::Normalizable).IsValid());
    // Double-precision squared length keeps both extremes normalizable.
    assert(ValidateQuaternion(pxr::GfQuatf(std::numeric_limits<float>::denorm_min()),
                              "q",
                              QuaternionValidationPolicy::Normalizable)
               .IsValid());
    assert(ValidateQuaternion(pxr::GfQuatf(std::numeric_limits<float>::max()),
                              "q",
                              QuaternionValidationPolicy::Normalizable)
               .IsValid());
    assert(ValidateQuaternion(pxr::GfQuatf(-1.0f), "q").IsValid());
    assert(ValidateQuaternion(pxr::GfQuatf(1.0000001f), "q").IsValid());
    assert(!ValidateQuaternion(pxr::GfQuatf(1.00001f), "q").IsValid());
    assert(!ValidateQuaternion(pxr::GfQuatf(1.0f, pxr::GfVec3f(0, inf, 0)), "q").IsValid());

    pose = MotionPose{};
    pose.root.hasPosition = pose.root.hasOrientation = true;
    pose.root.hasLinearVelocity = pose.root.hasAngularVelocity = true;
    pose.root.worldPosition[0] = nan;
    pose.root.worldOrientation = pxr::GfQuatf(0.0f);
    pose.root.linearVelocity[1] = inf;
    pose.root.angularVelocity[2] = nan;
    pose.lookAtTarget = pxr::GfVec3f(inf);
    pose.metadata.sourceTimestamp = inf;
    pose.timestamp = nan;
    report = ValidateMotionPose(pose);
    assert(report.reported.size() == 7);
    assert(Has(report, ValidationCode::NonFiniteValue, "timestamp"));
    assert(Has(report, ValidationCode::NonFiniteValue, "root.worldPosition"));
    assert(Has(report, ValidationCode::InvalidQuaternion, "root.worldOrientation"));
    assert(Has(report, ValidationCode::NonFiniteValue, "root.linearVelocity"));
    assert(Has(report, ValidationCode::NonFiniteValue, "root.angularVelocity"));
    assert(Has(report, ValidationCode::NonFiniteValue, "lookAtTarget"));
    assert(Has(report, ValidationCode::NonFiniteValue, "metadata.sourceTimestamp"));

    pose = MotionPose{};
    pose.channels.Set("custom", 1.5f); // Weights are deliberately not clamped.
    pose.confidence.emplace();
    pose.confidence->fill(0.0f);
    (*pose.confidence)[hips] = 1.0f;
    assert(ValidateMotionPose(pose).IsValid());
    (*pose.confidence)[hips] = 1.01f;
    assert(Has(ValidateMotionPose(pose), ValidationCode::ConfidenceRange, "confidence.hips"));
    (*pose.confidence)[hips] = nan;
    assert(Has(ValidateMotionPose(pose), ValidationCode::ConfidenceRange, "confidence.hips"));
    (*pose.confidence)[hips] = -0.01f;
    assert(Has(ValidateMotionPose(pose), ValidationCode::ConfidenceRange, "confidence.hips"));
    pose.channels.entries = {{"z", 1}, {"a", nan}, {"z", 2}, {"", 0}};
    report = ValidateMotionPose(pose);
    assert(Has(report, ValidationCode::ChannelOrder, "channels[1].name"));
    assert(Has(report, ValidationCode::DuplicateChannel, "channels[2].name"));
    assert(Has(report, ValidationCode::EmptyChannelName, "channels[3].name"));
    assert(Has(report, ValidationCode::NonFiniteValue, "channels[1].value"));
    pose.contacts = ContactState{};
    pose.contacts->leftFoot = static_cast<FootContact>(255);
    pose.contacts->rightFoot = static_cast<FootContact>(255);
    pose.metadata.kind = static_cast<MotionSourceKind>(255);
    report = ValidateMotionPose(pose);
    assert(Has(report, ValidationCode::InvalidEnum, "contacts.leftFoot"));
    assert(Has(report, ValidationCode::InvalidEnum, "contacts.rightFoot"));
    assert(Has(report, ValidationCode::InvalidEnum, "metadata.kind"));

    MotionClip clip;
    assert(ValidateMotionClip(clip).IsValid());
    clip.samples = {MotionPose{}, MotionPose{}};
    clip.samples[0].timestamp = clip.samples[1].timestamp = -2;
    assert(ValidateMotionClip(clip).IsValid());
    report = ValidateMotionClip(clip, ClipTimestampOrder::StrictlyIncreasing);
    assert(Has(report, ValidationCode::TimestampOrder, "samples[1].timestamp"));
    clip.samples[1].timestamp = -3;
    assert(Has(ValidateMotionClip(clip), ValidationCode::TimestampOrder, "samples[1].timestamp"));
    clip.samples[0].timestamp = -std::numeric_limits<double>::max();
    clip.samples[1].timestamp = std::numeric_limits<double>::max();
    assert(
        Has(ValidateMotionClip(clip), ValidationCode::NonFiniteValue, "samples[1].timestampSpan"));
    clip.samples[1].timestamp = std::numeric_limits<double>::infinity();
    assert(Has(ValidateMotionClip(clip), ValidationCode::NonFiniteValue, "samples[1].timestamp"));
    clip = MotionClip{};
    clip.startTime = nan;
    clip.endTime = inf;
    clip.nominalFrameRate = nan;
    clip.source.sourceTimestamp = inf;
    assert(ValidateMotionClip(clip).reported.size() == 4);
    clip = MotionClip{};
    assert(!ValidateMotionClip(clip, static_cast<ClipTimestampOrder>(255)).IsValid());
    assert(!ValidateMotionClip(clip,
                               ClipTimestampOrder::NonDecreasing,
                               static_cast<QuaternionValidationPolicy>(255))
                .IsValid());
    assert(
        !ValidateMotionPose(MotionPose{}, static_cast<QuaternionValidationPolicy>(255)).IsValid());

    ValidationReport merged;
    merged.Report(ValidationCode::NonFiniteValue, "a", "first");
    merged.Report(ValidationCode::NonFiniteValue, "a", "second");
    merged.Merge(merged);
    assert(merged.reported.size() == 1 && merged.reported[0].detail == "first");
    merged.Merge(merged, "child.");
    assert(merged.reported.size() == 2 && merged.reported[1].subject == "child.a");
    std::set<std::string_view> codes;
    for (unsigned i = 0; i < static_cast<unsigned>(ValidationCode::Count); ++i) {
        const auto code = ValidationCodeString(static_cast<ValidationCode>(i));
        assert(!code.empty() && codes.insert(code).second);
    }
    assert(ValidationCodeString(ValidationCode::Count).empty());
    assert(ValidationCodeString(static_cast<ValidationCode>(255)).empty());
    assert(ValidationCodeString(ValidationCode::HierarchyCycle) ==
           "MOTION_VALIDATION_HIERARCHY_CYCLE");
}
