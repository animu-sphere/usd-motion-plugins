// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionSampling/api.h"

#include "motionCore/MotionPose.h"

#include <optional>

namespace openstrata::motion
{

// Frame-rate independent exponential smoothing.
//
// The cutoff is expressed in hertz rather than as a blend weight so the same
// options behave identically on a 30 Hz clip and a 90 Hz live source: the
// per-step weight is derived from the actual elapsed time between poses.
class MOTIONSAMPLING_API PoseFilter
{
  public:
    struct Options
    {
        // Higher cutoff = more responsive, less smoothing. A non-positive
        // cutoff disables filtering and passes poses through unchanged.
        float cutoffHz = 6.0f;
        bool filterRootPosition = true;
        bool filterRootOrientation = true;
    };

    // One smoothing step, and everything it needs is passed in: the answer
    // and the state the next step needs.
    struct StepResult
    {
        // What `Apply` returns.
        MotionPose pose;
        // What `Apply` keeps. It is strictly richer than `pose`: a joint the
        // pose did not report is absent from `pose` and keeps its last smoothed
        // rotation here, which is how a brief dropout survives. A caller that
        // carries `pose` forward instead of `state` loses that history
        // (measured by usd-vrm-plugins' OpenExec filter node: a joint returning
        // after one missing frame comes back at 45.0 degrees rather than the
        // streamed 23.8).
        MotionPose state;
    };

    // `pose` smoothed against `state` -- the streaming filter as a pure
    // function, for a caller that holds the recurrence itself (an OpenExec
    // computation, whose driver carries one previous answer per prim). `Apply`
    // is this function over the object's own state, so the two cannot differ.
    //
    // A null `state` seeds: the pose passes through and becomes the state. So
    // does a non-positive cutoff, and a `pose` not later than `state`.
    static StepResult Step(const MotionPose* state, const MotionPose& pose,
                           const Options& options);

    PoseFilter() = default;
    explicit PoseFilter(const Options& options) : _options(options)
    {
    }

    const Options&
    GetOptions() const noexcept
    {
        return _options;
    }
    void
    SetOptions(const Options& options)
    {
        _options = options;
    }

    // Forgets the accumulated state; the next pose passes through untouched and
    // becomes the new seed. Call this on a source switch or a seek.
    void
    Reset() noexcept
    {
        _state.reset();
    }
    bool
    HasState() const noexcept
    {
        return _state.has_value();
    }

    // Smooths `pose` against the accumulated state and returns the result. A
    // joint absent from `pose` is not invented from history: it stays absent,
    // and its stored state is left untouched so a brief dropout does not
    // restart the filter for that joint.
    //
    // A non-increasing timestamp yields no smoothing step (the pose is
    // returned unchanged and reseeds the state).
    MotionPose Apply(const MotionPose& pose);

  private:
    Options _options;
    std::optional<MotionPose> _state;
};

} // namespace openstrata::motion
