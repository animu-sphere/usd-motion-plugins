// SPDX-License-Identifier: Apache-2.0
//
// Compiled against every installed package's public header (includes.h, which
// CMakeLists.txt generates from packages.json) and linked against every
// exported target. It prints how many it consumed, which the lane compares
// with packages.json, so a package that was silently skipped cannot pass.
#include "includes.h"
#include "motionUsd/SkeletonAnimationWriter.h"
#include "motionCore/Validation.h"
#include "motionRetarget/Validation.h"

#include <cstdio>

int
main()
{
    // Exercise the new installed header and symbol, including its null-stage
    // refusal, so merely adding a header without shipping the implementation
    // cannot pass the installed-consumer gate.
    if (openstrata::motion::AuthorSkeletonAnimation({}, {}, {}, {}, {}, nullptr)) {
        return 1;
    }
    using namespace openstrata::motion;
    MotionClip clip;
    clip.samples.emplace_back();
    if (!ValidateMotionClip(clip).IsValid())
        return 2;
    clip.samples[0].validRotations.set(0);
    clip.samples[0].localRotations[0] = pxr::GfQuatf(0.0f);
    const auto invalid = ValidateMotionClip(clip);
    if (invalid.IsValid() || invalid.reported[0].subject != "samples[0].localRotations.hips" ||
        ValidationCodeString(invalid.reported[0].code) != "MOTION_VALIDATION_INVALID_QUATERNION")
        return 3;
    const auto partial = ValidateRetargetConfiguration({}, {}, {});
    if (!partial.IsValid() || partial.diagnostics != DiagnoseRig({}, {}, {}))
        return 4;
    SourceRestPose cyclic;
    cyclic.parents[0] = 0;
    if (ValidateSourceRestPose(cyclic).IsValid())
        return 5;
    std::printf("consumed %d package(s)\n", USDMOTION_CONSUMER_PACKAGES);
    return 0;
}
