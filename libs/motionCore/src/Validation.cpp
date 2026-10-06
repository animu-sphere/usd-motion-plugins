// SPDX-License-Identifier: Apache-2.0
#include "motionCore/Validation.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace openstrata::motion {
namespace {
constexpr std::array<std::string_view, static_cast<std::size_t>(ValidationCode::Count)> codes = {
    "MOTION_VALIDATION_NON_FINITE_VALUE",
    "MOTION_VALIDATION_INVALID_QUATERNION",
    "MOTION_VALIDATION_NON_UNIT_QUATERNION",
    "MOTION_VALIDATION_TIMESTAMP_ORDER",
    "MOTION_VALIDATION_EMPTY_CHANNEL_NAME",
    "MOTION_VALIDATION_CHANNEL_ORDER",
    "MOTION_VALIDATION_DUPLICATE_CHANNEL",
    "MOTION_VALIDATION_CONFIDENCE_RANGE",
    "MOTION_VALIDATION_INVALID_ENUM",
    "MOTION_VALIDATION_INVALID_PARENT",
    "MOTION_VALIDATION_HIERARCHY_CYCLE",
    "MOTION_VALIDATION_HIERARCHY_ORDER",
    "MOTION_VALIDATION_EMPTY_JOINT_TOKEN",
    "MOTION_VALIDATION_DUPLICATE_JOINT_TOKEN",
    "MOTION_VALIDATION_INVALID_JOINT_INDEX",
    "MOTION_VALIDATION_TARGET_REST_SIZE",
};

void
CheckFinite(ValidationReport& report, double value, const std::string& subject)
{
    if (!std::isfinite(value))
        report.Report(ValidationCode::NonFiniteValue, subject, "Value must be finite");
}

void
CheckMetadata(ValidationReport& report, const SourceMetadata& source, const std::string& subject)
{
    if (source.kind > MotionSourceKind::Simulated)
        report.Report(ValidationCode::InvalidEnum, subject + ".kind", "Unknown motion source kind");
    if (source.sourceTimestamp)
        CheckFinite(report, *source.sourceTimestamp, subject + ".sourceTimestamp");
}
} // namespace

std::string_view
ValidationCodeString(ValidationCode code) noexcept
{
    const auto index = static_cast<std::size_t>(code);
    return index < codes.size() ? codes[index] : std::string_view{};
}

void
ValidationReport::Report(ValidationCode code, std::string subject, std::string detail)
{
    if (std::none_of(reported.begin(), reported.end(), [&](const ValidationDiagnostic& d) {
            return d.code == code && d.subject == subject;
        }))
        reported.push_back({code, std::move(subject), std::move(detail)});
}

void
ValidationReport::Merge(const ValidationReport& other, std::string_view prefix)
{
    // A self-merge with a prefix is useful and must not invalidate iteration.
    if (this == &other) {
        const auto copy = other;
        Merge(copy, prefix);
        return;
    }
    for (const auto& d : other.reported)
        Report(d.code, std::string(prefix) + d.subject, d.detail);
}

ValidationReport
ValidateQuaternion(const pxr::GfQuatf& value, std::string_view subject,
                   QuaternionValidationPolicy policy)
{
    ValidationReport report;
    if (policy != QuaternionValidationPolicy::Unit &&
        policy != QuaternionValidationPolicy::Normalizable) {
        report.Report(ValidationCode::InvalidEnum, "quaternionPolicy", "Unknown quaternion policy");
        return report;
    }
    const auto& v = value.GetImaginary();
    const double w = value.GetReal();
    const double length2 = w * w + double(v[0]) * v[0] + double(v[1]) * v[1] + double(v[2]) * v[2];
    if (!std::isfinite(length2) || length2 == 0)
        report.Report(ValidationCode::InvalidQuaternion,
                      std::string(subject),
                      "Quaternion must be finite and nonzero");
    else if (policy == QuaternionValidationPolicy::Unit && std::abs(length2 - 1.0) > 1e-6)
        report.Report(ValidationCode::NonUnitQuaternion,
                      std::string(subject),
                      "Quaternion must have unit length");
    return report;
}

ValidationReport
ValidateVector(const pxr::GfVec3f& value, std::string_view subject)
{
    ValidationReport report;
    if (!std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2]))
        report.Report(
            ValidationCode::NonFiniteValue, std::string(subject), "Vector must be finite");
    return report;
}

ValidationReport
ValidateMotionPose(const MotionPose& pose, QuaternionValidationPolicy policy)
{
    ValidationReport report;
    // Check the option even when no rotation is present.
    report.Merge(ValidateQuaternion(pxr::GfQuatf(1.0f), "", policy));
    CheckFinite(report, pose.timestamp, "timestamp");
    if (pose.root.hasPosition)
        report.Merge(ValidateVector(pose.root.worldPosition, "root.worldPosition"));
    if (pose.root.hasOrientation)
        report.Merge(
            ValidateQuaternion(pose.root.worldOrientation, "root.worldOrientation", policy));
    if (pose.root.hasLinearVelocity)
        report.Merge(ValidateVector(pose.root.linearVelocity, "root.linearVelocity"));
    if (pose.root.hasAngularVelocity)
        report.Merge(ValidateVector(pose.root.angularVelocity, "root.angularVelocity"));
    for (std::size_t i = 0; i < HumanJointCount; ++i) {
        const std::string bone(HumanJointName(static_cast<HumanJoint>(i)));
        if (pose.validRotations[i])
            report.Merge(
                ValidateQuaternion(pose.localRotations[i], "localRotations." + bone, policy));
        if (pose.confidence) {
            const float value = (*pose.confidence)[i];
            if (!std::isfinite(value) || value < 0 || value > 1)
                report.Report(ValidationCode::ConfidenceRange,
                              "confidence." + bone,
                              "Confidence must be finite and in [0,1]");
        }
    }
    if (pose.contacts) {
        if (pose.contacts->leftFoot > FootContact::InContact)
            report.Report(
                ValidationCode::InvalidEnum, "contacts.leftFoot", "Unknown contact state");
        if (pose.contacts->rightFoot > FootContact::InContact)
            report.Report(
                ValidationCode::InvalidEnum, "contacts.rightFoot", "Unknown contact state");
    }
    if (pose.lookAtTarget)
        report.Merge(ValidateVector(*pose.lookAtTarget, "lookAtTarget"));
    std::set<std::string> names;
    const auto& channels = pose.channels.entries;
    for (std::size_t i = 0; i < channels.size(); ++i) {
        const auto& channel = channels[i];
        const std::string subject = "channels[" + std::to_string(i) + "]";
        if (channel.name.empty())
            report.Report(
                ValidationCode::EmptyChannelName, subject + ".name", "Channel must have a name");
        if (!names.insert(channel.name).second)
            report.Report(ValidationCode::DuplicateChannel,
                          subject + ".name",
                          "Channel names must be unique");
        if (i && channel.name < channels[i - 1].name)
            report.Report(
                ValidationCode::ChannelOrder, subject + ".name", "Channel names must be sorted");
        CheckFinite(report, channel.value, subject + ".value");
    }
    CheckMetadata(report, pose.metadata, "metadata");
    return report;
}

ValidationReport
ValidateMotionClip(const MotionClip& clip, ClipTimestampOrder order,
                   QuaternionValidationPolicy policy)
{
    ValidationReport report;
    report.Merge(ValidateQuaternion(pxr::GfQuatf(1.0f), "", policy));
    if (order != ClipTimestampOrder::NonDecreasing &&
        order != ClipTimestampOrder::StrictlyIncreasing)
        report.Report(
            ValidationCode::InvalidEnum, "timestampOrder", "Unknown timestamp ordering policy");
    CheckFinite(report, clip.startTime, "startTime");
    CheckFinite(report, clip.endTime, "endTime");
    CheckFinite(report, clip.nominalFrameRate, "nominalFrameRate");
    CheckMetadata(report, clip.source, "source");
    for (std::size_t i = 0; i < clip.samples.size(); ++i) {
        const auto& pose = clip.samples[i];
        const std::string prefix = "samples[" + std::to_string(i) + "].";
        report.Merge(ValidateMotionPose(pose, policy), prefix);
        if (i && std::isfinite(pose.timestamp) && std::isfinite(clip.samples[i - 1].timestamp)) {
            const double previous = clip.samples[i - 1].timestamp;
            if (pose.timestamp < previous ||
                (order == ClipTimestampOrder::StrictlyIncreasing && pose.timestamp == previous))
                report.Report(ValidationCode::TimestampOrder,
                              prefix + "timestamp",
                              "Sample timestamps violate ordering policy");
            CheckFinite(report, pose.timestamp - previous, prefix + "timestampSpan");
        }
    }
    return report;
}
} // namespace openstrata::motion
