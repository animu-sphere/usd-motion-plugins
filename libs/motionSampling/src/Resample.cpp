// SPDX-License-Identifier: Apache-2.0
#include "motionSampling/Resample.h"

#include "Bracket.h"

#include <cmath>

namespace openstrata::motion
{

MotionPose
SampleAnimation(const MotionClip& animation, double timestamp)
{
    return detail::SampleBracketed(animation.samples.begin(), animation.samples.end(), timestamp)
        .value_or(MotionPose());
}

MotionClip
Resample(const MotionClip& animation, double frameRate)
{
    MotionClip result;
    result.startTime = animation.startTime;
    result.endTime = animation.endTime;
    result.nominalFrameRate = animation.nominalFrameRate;
    result.source = animation.source;

    if (animation.samples.empty() || frameRate <= 0.0)
    {
        return result;
    }

    result.nominalFrameRate = frameRate;
    double start = animation.startTime;
    double end = animation.endTime;
    if (end <= start)
    {
        // A caller that filled `samples` but left the declared interval at its
        // default would otherwise collapse the whole clip to a single pose. The
        // samples are the authority on what the clip actually spans.
        start = animation.samples.front().timestamp;
        end = animation.samples.back().timestamp;
        result.startTime = start;
        result.endTime = end;
    }
    if (end <= start)
    {
        result.samples.push_back(SampleAnimation(animation, start));
        result.samples.back().timestamp = start;
        return result;
    }

    const double step = 1.0 / frameRate;
    const auto steps = static_cast<std::size_t>(std::floor((end - start) / step + 1e-9));
    result.samples.reserve(steps + 2);
    for (std::size_t i = 0; i <= steps; ++i)
    {
        const double time = start + static_cast<double>(i) * step;
        MotionPose pose = SampleAnimation(animation, time);
        pose.timestamp = time;
        result.samples.push_back(std::move(pose));
    }
    // The last uniform step lands short of `end` whenever the duration is not
    // an exact multiple of the step. Emit the real end so the resampled clip
    // covers the same interval as its source.
    if (result.samples.back().timestamp < end)
    {
        MotionPose pose = SampleAnimation(animation, end);
        pose.timestamp = end;
        result.samples.push_back(std::move(pose));
    }
    return result;
}

} // namespace openstrata::motion
