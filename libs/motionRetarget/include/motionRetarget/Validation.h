// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionCore/Validation.h"
#include "motionRetarget/PoseRetargeter.h"

namespace openstrata::motion {

MOTIONRETARGET_API ValidationReport
ValidateSkeletonDescriptor(const SkeletonDescriptor& skeleton,
                           QuaternionValidationPolicy policy = QuaternionValidationPolicy::Unit);
MOTIONRETARGET_API ValidationReport
ValidateSourceRestPose(const SourceRestPose& rest,
                       QuaternionValidationPolicy policy = QuaternionValidationPolicy::Unit);

struct RetargetValidationReport {
    ValidationReport values;
    // Existing recoverable rig warnings, including caller-required bones and
    // duplicate targets. A legal partial rig does not become malformed input.
    RetargetDiagnostics diagnostics;
    bool IsValid() const noexcept { return values.IsValid(); }
};

// Validate before building a retargeter from mutable values. No input is
// normalized or repaired. Rig policy diagnostics match DiagnoseRig exactly.
MOTIONRETARGET_API RetargetValidationReport
ValidateRetargetConfiguration(const SkeletonDescriptor& skeleton, const RetargetMap& map,
                              const SourceRestPose& sourceRest, const RetargetOptions& options = {},
                              QuaternionValidationPolicy policy = QuaternionValidationPolicy::Unit);

} // namespace openstrata::motion
