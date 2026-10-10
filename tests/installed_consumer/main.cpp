// SPDX-License-Identifier: Apache-2.0
//
// Compiled against every installed package's public header (includes.h, which
// CMakeLists.txt generates from packages.json) and linked against every
// exported target. It prints how many it consumed, which the lane compares
// with packages.json, so a package that was silently skipped cannot pass.
#include "includes.h"
#include "motionUsd/SkeletonAnimationWriter.h"
#include "motionUsd/SkeletonReader.h"
#include "motionCore/Validation.h"
#include "motionRetarget/Validation.h"
#include "motionRecording/LiveCaptureSource.h"
#include "motion_inputs.h"

#include <cstdio>
#include <limits>

int
main()
{
    if (!CheckMotionInputs())
        return 6;
    // Exercise the new installed header and symbol, including its null-stage
    // refusal, so merely adding a header without shipping the implementation
    // cannot pass the installed-consumer gate.
    if (openstrata::motion::AuthorSkeletonAnimation({}, {}, {}, {}, {}, nullptr)) {
        return 1;
    }
    using namespace openstrata::motion;
    LiveCaptureSource live;
    MotionPose observation;
    // Acquisition names are provenance values, never processing selectors or
    // a reason to depend on a connector/transport package.
    observation.metadata.provider = "mocopi";
    observation.metadata.protocol = "websocket";
    observation.metadata.sourceId = "actor-1";
    observation.metadata.kind = MotionSourceKind::LiveCapture;
    live.SetSourceMetadata(observation.metadata);
    observation.metadata.sourceTimestamp = 42.0;
    observation.metadata.sequenceNumber = 7;
    observation.root.hasPosition = true;
    observation.timestamp = std::numeric_limits<double>::quiet_NaN();
    if (live.Push(observation) || !live.IsEmpty() ||
        live.GetStats().framesRejectedInvalidTimestamp != 1)
        return 7;
    observation.timestamp = 100.0;
    if (!live.Push(observation) || !live.AlignClock(0.0))
        return 8;
    if (live.AlignClock(std::numeric_limits<double>::infinity()) ||
        live.GetClockOffset() != 100.0 || live.Sample(0.0).status != PoseSampleStatus::Sampled ||
        !live.Sample(0.0).pose || live.Sample(0.0).pose->metadata != observation.metadata)
        return 9;
    live.Reset();
    observation.timestamp = -1.0;
    if (!live.Push(observation) || live.GetBuffer().GetNewest().root.hasLinearVelocity ||
        !live.AlignClock(5.0) || live.Sample(5.0).status != PoseSampleStatus::Sampled)
        return 10;
    SkeletonStageRead skeleton;
    SkeletonReadDiagnostic readingDiagnostic;
    const pxr::SdfPath skeletonPath("/Skeleton");
    if (ReadSkeleton({}, skeletonPath, &skeleton, &readingDiagnostic) ||
        readingDiagnostic.code != "MOTION_USD_STAGE" ||
        readingDiagnostic.subject != skeletonPath.GetString())
        return 4;
    MotionStageRead motion;
    if (ReadCanonicalMotionStage({}, skeletonPath, &motion, &readingDiagnostic) ||
        readingDiagnostic.code != "MOTION_USD_STAGE")
        return 5;
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
