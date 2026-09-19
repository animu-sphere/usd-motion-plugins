// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionSampling/api.h"

#include "motionCore/MotionPose.h"

namespace openstrata::motion
{

// Resamples an animation onto a uniform timeline at `frameRate` Hz spanning
// [startTime, endTime]. The endpoints are always emitted, so a clip whose
// duration is not an exact multiple of the step still ends on its last sample.
// A degenerate declared interval falls back to the sample timestamps, so a
// caller that filled `samples` and left startTime/endTime at their defaults
// gets its whole clip back rather than a single collapsed pose.
//
// Returns an animation with no samples when the input has none or when
// `frameRate` is not positive; source metadata and the time range are carried
// through unchanged so provenance survives a resample.
MOTIONSAMPLING_API MotionClip Resample(const MotionClip& animation, double frameRate);

// Samples an animation at an arbitrary time using the same hold-at-the-edges
// rule as PoseBuffer::Sample. Returns a default-constructed pose when the
// animation has no samples.
MOTIONSAMPLING_API MotionPose SampleAnimation(const MotionClip& animation,
                                               double timestamp);

} // namespace openstrata::motion
