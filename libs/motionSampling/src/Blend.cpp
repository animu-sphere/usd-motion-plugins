// SPDX-License-Identifier: Apache-2.0
#include "motionSampling/Blend.h"

#include "motionSampling/Interpolation.h"

#include <algorithm>

namespace openstrata::motion
{

MotionPose
BlendPoses(const MotionPose& a, const MotionPose& b, float weight)
{
    return LerpPose(a, b, weight);
}

MotionPose
BlendPoses(const std::vector<WeightedPose>& poses)
{
    MotionPose result;
    double accumulated = 0.0;
    bool seeded = false;

    for (const WeightedPose& entry : poses)
    {
        const double weight = std::max(entry.weight, 0.0f);
        if (weight <= 0.0)
        {
            continue;
        }
        if (!seeded)
        {
            result = entry.pose;
            accumulated = weight;
            seeded = true;
            continue;
        }
        // Fold each pose in at its share of the running total. Successive
        // pairwise slerps keep every intermediate a unit quaternion, which a
        // component-wise weighted sum would not.
        accumulated += weight;
        result = LerpPose(result, entry.pose, static_cast<float>(weight / accumulated));
    }

    return result;
}

} // namespace openstrata::motion
