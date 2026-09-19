// SPDX-License-Identifier: Apache-2.0
#include "motionSampling/Filter.h"

#include "motionSampling/Interpolation.h"

#include <cmath>

namespace openstrata::motion
{
namespace
{

constexpr float kTwoPi = 6.2831853071795862f;

// Exponential smoothing weight for a step of `dt` seconds at `cutoffHz`. The
// time constant form keeps the response identical regardless of sample rate.
float
SmoothingAlpha(float cutoffHz, double dt)
{
    if (cutoffHz <= 0.0f || dt <= 0.0)
    {
        return 1.0f;
    }
    const float exponent = -kTwoPi * cutoffHz * static_cast<float>(dt);
    return 1.0f - std::exp(exponent);
}

} // namespace

PoseFilter::StepResult
PoseFilter::Step(const MotionPose* state, const MotionPose& pose, const Options& options)
{
    if (options.cutoffHz <= 0.0f || !state)
    {
        return StepResult{pose, pose};
    }

    const double dt = pose.timestamp - state->timestamp;
    if (dt <= 0.0)
    {
        return StepResult{pose, pose};
    }

    const float alpha = SmoothingAlpha(options.cutoffHz, dt);
    StepResult step{pose, MotionPose()};
    MotionPose& result = step.pose;

    for (std::size_t i = 0; i < HumanJointCount; ++i)
    {
        if (!pose.validRotations.test(i))
        {
            // A dropout leaves both the output joint and the retained state
            // alone, so the filter resumes from the last real sample instead of
            // restarting when the joint comes back.
            continue;
        }
        if (!state->validRotations.test(i))
        {
            continue;
        }
        result.localRotations[i] =
            SlerpShortest(state->localRotations[i], pose.localRotations[i], alpha);
    }

    if (options.filterRootPosition && pose.root.hasPosition && state->root.hasPosition)
    {
        result.root.worldPosition = state->root.worldPosition +
                                    (pose.root.worldPosition - state->root.worldPosition) * alpha;
    }
    if (options.filterRootOrientation && pose.root.hasOrientation && state->root.hasOrientation)
    {
        result.root.worldOrientation =
            SlerpShortest(state->root.worldOrientation, pose.root.worldOrientation, alpha);
    }

    // Carry forward the joints this pose did not report so their history
    // survives the dropout.
    step.state = result;
    for (std::size_t i = 0; i < HumanJointCount; ++i)
    {
        if (!pose.validRotations.test(i) && state->validRotations.test(i))
        {
            step.state.localRotations[i] = state->localRotations[i];
            step.state.validRotations.set(i);
        }
    }
    return step;
}

MotionPose
PoseFilter::Apply(const MotionPose& pose)
{
    StepResult step = Step(_state ? &*_state : nullptr, pose, _options);
    _state = std::move(step.state);
    return std::move(step.pose);
}

} // namespace openstrata::motion
