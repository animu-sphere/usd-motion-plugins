// SPDX-License-Identifier: Apache-2.0
//
// The one bracket-and-hold search. A clip (`SampleClip`, `SampleAnimation`) and
// a buffer (`PoseBuffer::Sample`) answer the same question over two containers,
// and before this header each wrote the search out on its own.
#pragma once

#include "motionSampling/Interpolation.h"

#include "motionCore/MotionPose.h"

#include <algorithm>
#include <iterator>
#include <optional>

namespace openstrata::motion::detail
{

// The pose `[first, last)` states at `timestamp`: the two bracketing samples
// interpolated by `LerpPose`, or the nearer boundary sample held unchanged
// outside the range. Nullopt only for an empty range.
//
// Precondition: timestamps are finite and never decrease. The search is binary,
// so a range out of order answers with a bracket nobody measured. Repeated
// timestamps are allowed, and every answer is still a sample or an
// interpolation between neighbours: a request at or past the end holds the
// last of a repeated pair, one inside lands on the first.
template <class Iterator>
std::optional<MotionPose>
SampleBracketed(Iterator first, Iterator last, double timestamp)
{
    if (first == last)
    {
        return std::nullopt;
    }
    const Iterator back = std::prev(last);
    if (timestamp <= first->timestamp)
    {
        return *first;
    }
    if (timestamp >= back->timestamp)
    {
        return *back;
    }

    // The first sample at or after `timestamp` is the upper bracket; the range
    // checks above keep it strictly inside.
    const Iterator upper = std::lower_bound(first, last, timestamp,
                                            [](const MotionPose& sample, double time)
                                            { return sample.timestamp < time; });
    const Iterator lower = std::prev(upper);

    const double span = upper->timestamp - lower->timestamp;
    if (span <= 0.0)
    {
        return *lower;
    }
    const float alpha = static_cast<float>((timestamp - lower->timestamp) / span);
    return LerpPose(*lower, *upper, alpha);
}

} // namespace openstrata::motion::detail
