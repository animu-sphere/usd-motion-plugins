// SPDX-License-Identifier: Apache-2.0
#include "motionSampling/Blend.h"

#include "motionSampling/Interpolation.h"

namespace openstrata::motion
{

MotionPose
BlendPoses(const MotionPose& a, const MotionPose& b, float weight)
{
    return LerpPose(a, b, weight);
}

std::optional<MotionPose>
BlendPoses(const std::vector<WeightedPose>& poses)
{
    std::optional<MotionPose> result;
    double accumulated = 0.0;

    for (const WeightedPose& entry : poses)
    {
        // Written so a NaN fails it too: no weight, like a negative one.
        if (!(entry.weight > 0.0f))
        {
            continue;
        }
        const double weight = entry.weight;
        if (!result)
        {
            result = entry.pose;
            accumulated = weight;
            continue;
        }
        // Fold each pose in at its share of the running total. Successive
        // pairwise slerps keep every intermediate a unit quaternion, which a
        // component-wise weighted sum would not. The instant is the first
        // weighted pose's: `LerpPose` interpolates timestamps, and sources
        // combined at one instant are not two samples in time.
        accumulated += weight;
        const double instant = result->timestamp;
        result = LerpPose(*result, entry.pose, static_cast<float>(weight / accumulated));
        result->timestamp = instant;
    }

    return result;
}

} // namespace openstrata::motion
