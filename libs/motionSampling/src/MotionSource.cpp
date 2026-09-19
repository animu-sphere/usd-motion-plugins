// SPDX-License-Identifier: Apache-2.0
#include "motionSampling/MotionSource.h"

#include "Bracket.h"

namespace openstrata::motion
{

IMotionSource::~IMotionSource() = default;

const char*
PoseSampleStatusName(PoseSampleStatus status) noexcept
{
    switch (status)
    {
    case PoseSampleStatus::Unavailable:
        return "unavailable";
    case PoseSampleStatus::Sampled:
        return "sampled";
    case PoseSampleStatus::Held:
        return "held";
    case PoseSampleStatus::Extrapolated:
        return "extrapolated";
    }
    return "unavailable";
}

bool
operator==(const PoseSampleResult& a, const PoseSampleResult& b) noexcept
{
    return a.status == b.status && a.pose == b.pose && a.lag == b.lag;
}

bool
operator!=(const PoseSampleResult& a, const PoseSampleResult& b) noexcept
{
    return !(a == b);
}

PoseSampleResult
SampleClip(const MotionClip& clip, double timestamp)
{
    PoseSampleResult result;
    const std::vector<MotionPose>& samples = clip.samples;
    result.pose = detail::SampleBracketed(samples.begin(), samples.end(), timestamp);
    if (!result.pose)
    {
        return result;
    }

    const double first = samples.front().timestamp;
    const double last = samples.back().timestamp;
    result.status =
        (timestamp < first - PoseSampleTimeTolerance || timestamp > last + PoseSampleTimeTolerance)
            ? PoseSampleStatus::Held
            : PoseSampleStatus::Sampled;
    result.lag = timestamp - last;
    result.pose->timestamp = timestamp;
    return result;
}

ClipSource::ClipSource(MotionClip animation) : _animation(std::move(animation))
{
}

void
ClipSource::SetAnimation(MotionClip animation)
{
    _animation = std::move(animation);
}

PoseSampleResult
ClipSource::Sample(double evaluationTime)
{
    PoseSampleResult result = SampleClip(_animation, evaluationTime - _startOffset);
    // The pose is reported on the consumer's clock, not the clip's, so a
    // caller that pushes it into a buffer keeps one coherent timeline.
    if (result.pose)
    {
        result.pose->timestamp = evaluationTime;
    }
    return result;
}

SourceMetadata
ClipSource::GetSourceMetadata() const
{
    return _animation.source;
}

bool
ClipSource::GetTimeRange(double* startTime, double* endTime) const
{
    if (_animation.samples.empty())
    {
        return false;
    }
    if (startTime)
    {
        *startTime = _animation.samples.front().timestamp + _startOffset;
    }
    if (endTime)
    {
        *endTime = _animation.samples.back().timestamp + _startOffset;
    }
    return true;
}

} // namespace openstrata::motion
