// SPDX-License-Identifier: Apache-2.0
//
// Turns a stream back into a clip (design policy §14; MOTION_CONTRACT.md §10).
//
// This is the join between the live half of the motion layer and the offline
// half. A live source answers one evaluation time at a time; the retarget core,
// a clip writer, and every bake want a finished `MotionClip`. The
// recorder is what makes the two the same pipeline rather than two pipelines --
// and because it records the *status* of every tick alongside the pose, a clip
// baked from a laggy session carries the evidence of that lag instead of
// quietly looking fine.
#pragma once

#include "motionSampling/MotionSource.h"
#include "motionRecording/api.h"

#include "motionCore/MotionPose.h"

#include <cstddef>

namespace openstrata::motion
{

struct RecordReport
{
    std::size_t ticks = 0;
    std::size_t sampled = 0;
    std::size_t held = 0;
    std::size_t extrapolated = 0;
    std::size_t unavailable = 0;
    std::size_t rejected = 0;
    double peakLagSeconds = 0.0;

    // Ticks that produced a pose from real bracketing data. A session whose
    // `sampled` count is far below `ticks` was evaluated ahead of its data.
    std::size_t
    Recorded() const noexcept
    {
        return sampled + held + extrapolated;
    }
};

class MOTIONRECORDING_API MotionRecorder
{
  public:
    explicit MotionRecorder(double frameRate = 30.0);

    // Appends the pose the result carries. A tick the source could not answer
    // is counted and dropped, not padded with an invented pose. Returns false
    // when nothing was appended -- either the result was empty, or its
    // timestamp did not advance.
    bool Record(const PoseSampleResult& result);

    const RecordReport&
    GetReport() const noexcept
    {
        return _report;
    }
    std::size_t
    GetFrameCount() const noexcept
    {
        return _animation.samples.size();
    }

    // Stamps the time range, the nominal rate, and the source the first
    // recorded pose names -- not its stamp or counter, which stay on each
    // sample -- onto the clip, and hands it over. The frames are gone
    // afterwards; the report is not, so a caller can take the clip and still
    // say how the session that produced it went. Clear() drops both.
    MotionClip Take();

    void Clear();

  private:
    MotionClip _animation;
    RecordReport _report;
    double _frameRate;
};

} // namespace openstrata::motion
