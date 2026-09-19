// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionSampling/api.h"

#include "motionCore/MotionPose.h"

#include <optional>
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

// N-pose blend of sources sampled at one instant, or nullopt when there is
// nothing to blend: an empty list, or no weight above zero.
//
// Preconditions, which a caller checks and this function does not:
// - every weight is finite. A NaN counts as no weight, like a negative one;
//   an infinity makes the running total infinite and the result meaningless.
// - every weighted pose carries the same timestamp. The result is stamped at
//   the first weighted pose's, never at an interpolated instant: sources
//   combined at one instant are not two samples in time, which is what
//   `LerpPose`'s timestamp interpolation is for.
//
// The blend folds the poses in by successive pairwise interpolation, each at
// its share of the running total. That keeps every intermediate a unit
// quaternion, which a component-wise weighted sum does not -- and it makes the
// answer depend on the order of `poses` once three or more rotations differ
// (usd-vrm-plugins measured 4.247 degrees between three sources and their
// reverse). Two poses, or poses that agree, give the same answer either way.
// A caller that needs one answer fixes the order.
MOTIONSAMPLING_API std::optional<MotionPose> BlendPoses(const std::vector<WeightedPose>& poses);

} // namespace openstrata::motion
