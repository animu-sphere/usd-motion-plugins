// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionSampling/api.h"

#include "motionCore/MotionPose.h"

#include <vector>

namespace openstrata::motion
{

struct WeightedPose
{
    MotionPose pose;
    float weight = 1.0f;
};

// Two-pose blend. `weight` is clamped to [0, 1]: 0 yields `a`, 1 yields `b`.
// Joint validity follows LerpPose — a joint present in only one input is taken
// from that input rather than blended toward identity.
MOTIONSAMPLING_API MotionPose BlendPoses(const MotionPose& a, const MotionPose& b,
                                          float weight);

// N-pose blend by successive pairwise interpolation, which keeps every
// intermediate result a unit quaternion (a component-wise weighted sum does
// not). Negative weights are treated as zero; when the total weight is zero or
// the list is empty the result is a default-constructed pose.
MOTIONSAMPLING_API MotionPose BlendPoses(const std::vector<WeightedPose>& poses);

} // namespace openstrata::motion
