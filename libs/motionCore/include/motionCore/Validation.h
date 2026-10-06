// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionCore/MotionPose.h"

namespace openstrata::motion {

enum class ValidationCode : std::uint8_t {
    NonFiniteValue,
    InvalidQuaternion,
    NonUnitQuaternion,
    TimestampOrder,
    EmptyChannelName,
    ChannelOrder,
    DuplicateChannel,
    ConfidenceRange,
    InvalidEnum,
    InvalidParent,
    HierarchyCycle,
    HierarchyOrder,
    EmptyJointToken,
    DuplicateJointToken,
    InvalidJointIndex,
    TargetRestSize,
    Count,
};

// All validation codes denote malformed input: severity error, not
// recoverable. Retarget policy warnings keep their existing diagnostic codes.
MOTIONCORE_API std::string_view ValidationCodeString(ValidationCode code) noexcept;

struct ValidationDiagnostic {
    ValidationCode code;
    std::string subject;
    std::string detail;
};

struct ValidationReport {
    std::vector<ValidationDiagnostic> reported;

    bool IsValid() const noexcept { return reported.empty(); }
    // Keep each code/subject once, in first-raised order.
    MOTIONCORE_API void Report(ValidationCode code, std::string subject, std::string detail);
    MOTIONCORE_API void Merge(const ValidationReport& other, std::string_view prefix = {});
};

enum class QuaternionValidationPolicy : std::uint8_t {
    Unit,
    // Accept finite nonzero quaternions for an operation that explicitly
    // normalizes them. Validation itself never changes the input.
    Normalizable,
};

enum class ClipTimestampOrder : std::uint8_t {
    NonDecreasing,
    StrictlyIncreasing,
};

// Shared primitive checks used by motion-owned rest/configuration validation.
// Unit uses abs(squared length - 1) <= 1e-6 in double precision.
MOTIONCORE_API ValidationReport
ValidateQuaternion(const pxr::GfQuatf& value, std::string_view subject,
                   QuaternionValidationPolicy policy = QuaternionValidationPolicy::Unit);
MOTIONCORE_API ValidationReport ValidateVector(const pxr::GfVec3f& value, std::string_view subject);
MOTIONCORE_API ValidationReport ValidateMotionPose(
    const MotionPose& pose, QuaternionValidationPolicy policy = QuaternionValidationPolicy::Unit);
// Finite non-decreasing sample times match SampleClip's contract; stream-like
// callers may select strict ordering. Empty clips and sparse poses are valid.
MOTIONCORE_API ValidationReport ValidateMotionClip(
    const MotionClip& clip, ClipTimestampOrder order = ClipTimestampOrder::NonDecreasing,
    QuaternionValidationPolicy policy = QuaternionValidationPolicy::Unit);

// Values remain mutable. Validate after the last edit and before an operation
// requiring these invariants; a report is not a certificate for later edits.
} // namespace openstrata::motion
