// SPDX-License-Identifier: Apache-2.0
#include "motionRecording/LiveCaptureSource.h"

#include <algorithm>
#include <array>
#include <utility>

namespace openstrata::motion
{

namespace
{

bool
CarriesAnything(const MotionPose& pose)
{
    return pose.validRotations.any() || pose.root.hasPosition || pose.root.hasOrientation;
}

} // namespace

RootMotion
ConditionRootMotion(const MotionPose& prior, const MotionPose& pose, RootMotionIntake intake)
{
    switch (intake)
    {
    case RootMotionIntake::Ignore:
        return RootMotion();
    case RootMotionIntake::Passthrough:
        return pose.root;
    case RootMotionIntake::DeriveVelocity:
        break;
    }

    RootMotion root = pose.root;
    if (!root.hasPosition || root.hasLinearVelocity || !prior.root.hasPosition)
    {
        return root;
    }
    const double delta = pose.timestamp - prior.timestamp;
    if (delta > 0.0)
    {
        root.linearVelocity =
            (root.worldPosition - prior.root.worldPosition) / static_cast<float>(delta);
        root.hasLinearVelocity = true;
    }
    return root;
}

LiveCaptureSource::LiveCaptureSource(const LiveCaptureConfig& config)
    : _buffer(config.bufferCapacity)
{
    _metadata.kind = MotionSourceKind::LiveCapture;
    SetConfig(config);
}

void
LiveCaptureSource::SetConfig(const LiveCaptureConfig& config)
{
    _config = config;
    _buffer.SetCapacity(_config.bufferCapacity);

    PoseFilter::Options options;
    options.cutoffHz = _config.smoothingCutoffHz;
    _filter.SetOptions(options);
    _filter.Reset();
}

void
LiveCaptureSource::SetSourceMetadata(const SourceMetadata& metadata)
{
    _metadata = metadata;
    // The kind is not the connector's to choose: anything arriving through this
    // class is a live capture by construction (MOTION_CONTRACT.md §7).
    _metadata.kind = MotionSourceKind::LiveCapture;
    // Nor is a stamp or a counter the stream's: they belong to each sample, and
    // arrive on the pose a connector pushes (step 5 of Condition keeps them).
    _metadata.sourceTimestamp.reset();
    _metadata.sequenceNumber.reset();
}

MotionPose
LiveCaptureSource::_Condition(const MotionPose& pose)
{
    MotionPose conditioned = pose;

    // 1. Confidence gate. A frame that reports no confidence at all is trusted
    //    as given -- a connector that cannot measure confidence must not lose
    //    its joints for saying so.
    if (conditioned.confidence && _config.confidenceFloor > 0.0f)
    {
        const std::array<float, HumanJointCount>& scores = *conditioned.confidence;
        for (std::size_t joint = 0; joint < HumanJointCount; ++joint)
        {
            if (conditioned.validRotations.test(joint) && scores[joint] < _config.confidenceFloor)
            {
                conditioned.validRotations.reset(joint);
                ++_stats.jointsGatedByConfidence;
            }
        }
    }

    // 2. Whatever survived the gate is a real observation.
    for (std::size_t joint = 0; joint < HumanJointCount; ++joint)
    {
        if (conditioned.validRotations.test(joint))
        {
            _observedJoints.set(joint);
            ++_stats.jointsObserved;
        }
    }

    // 3. Resolve what is still missing. Holding is per joint and comes from the
    //    last accepted frame, so a dropout freezes one limb rather than
    //    reverting it toward rest -- the same invariant PoseBuffer keeps for a
    //    missing sample.
    for (std::size_t joint = 0; joint < HumanJointCount; ++joint)
    {
        if (conditioned.validRotations.test(joint))
        {
            continue;
        }
        if (_config.missingJoints == MissingJointPolicy::HoldLast && _lastAccepted &&
            _lastAccepted->validRotations.test(joint))
        {
            conditioned.localRotations[joint] = _lastAccepted->localRotations[joint];
            conditioned.validRotations.set(joint);
            ++_stats.jointsHeld;
        }
        else
        {
            ++_stats.jointsUnbound;
        }
    }

    // 4. Root motion.
    if (_config.rootMotion != RootMotionIntake::Ignore && conditioned.root.hasPosition)
    {
        ++_stats.rootSamplesObserved;
    }
    const bool reportedVelocity = conditioned.root.hasLinearVelocity;
    conditioned.root = ConditionRootMotion(_lastAccepted ? *_lastAccepted : conditioned,
                                           conditioned, _config.rootMotion);
    if (!reportedVelocity && conditioned.root.hasLinearVelocity)
    {
        ++_stats.rootVelocitiesDerived;
    }

    // 5. Provenance is stamped on every buffered pose, so a pose that outlives
    //    this object still says where it came from. The stream names the
    //    source; the stamp and the counter are the sample's own, so the ones
    //    the connector pushed are kept rather than overwritten.
    {
        const SourceMetadata pushed = conditioned.metadata;
        conditioned.metadata = _metadata;
        conditioned.metadata.sourceTimestamp = pushed.sourceTimestamp;
        conditioned.metadata.sequenceNumber = pushed.sequenceNumber;
    }

    // 6. Smoothing runs last, on the fully resolved frame, so a held joint is
    //    smoothed on the same terms as an observed one.
    if (_config.smoothingCutoffHz > 0.0f)
    {
        conditioned = _filter.Apply(conditioned);
    }
    return conditioned;
}

bool
LiveCaptureSource::Push(const MotionPose& pose)
{
    if (!CarriesAnything(pose))
    {
        ++_stats.framesRejectedEmpty;
        return false;
    }

    if (_lastAccepted)
    {
        const double delta = pose.timestamp - _lastAccepted->timestamp;
        if (delta <= 0.0)
        {
            if (_config.staleFrameSeconds > 0.0 && -delta > _config.staleFrameSeconds)
            {
                ++_stats.framesRejectedStale;
            }
            else
            {
                ++_stats.framesRejectedOutOfOrder;
            }
            return false;
        }
    }

    const MotionPose conditioned = _Condition(pose);
    if (!_buffer.Push(conditioned))
    {
        // Unreachable while _lastAccepted tracks the buffer head, but the
        // buffer -- not this class -- owns the ordering rule, so defer to it.
        ++_stats.framesRejectedOutOfOrder;
        return false;
    }

    _lastAccepted = conditioned;
    ++_stats.framesAccepted;
    return true;
}

bool
LiveCaptureSource::AlignClock(double evaluationTime) noexcept
{
    double newest = 0.0;
    if (!_buffer.GetTimeRange(nullptr, &newest))
    {
        return false;
    }
    _clockOffset = newest - evaluationTime;
    return true;
}

PoseSampleResult
LiveCaptureSource::Sample(double evaluationTime)
{
    PoseSampleResult result;

    double oldest = 0.0;
    double newest = 0.0;
    if (!_buffer.GetTimeRange(&oldest, &newest))
    {
        ++_stats.samplesUnavailable;
        return result;
    }

    const double captureTime = evaluationTime + _clockOffset;
    result.lag = captureTime - newest;
    _stats.peakLagSeconds = std::max(_stats.peakLagSeconds, result.lag);

    std::optional<MotionPose> pose =
        _config.maxExtrapolationSeconds > 0.0
            ? _buffer.SampleExtrapolated(captureTime, _config.maxExtrapolationSeconds)
            : _buffer.Sample(captureTime);
    if (!pose)
    {
        ++_stats.samplesUnavailable;
        return result;
    }

    // The boundary carries PoseSampleTimeTolerance: a request that lands on an
    // observed sample to within the timeline's own precision *is* that sample,
    // and reporting it as extrapolated would make a clean session look like a
    // failing one.
    if (captureTime < oldest - PoseSampleTimeTolerance ||
        captureTime > newest + PoseSampleTimeTolerance)
    {
        // Extrapolation only ever moves the root; when the newest frame had no
        // usable velocity nothing advanced and this is an ordinary hold. Report
        // what happened, not what was requested.
        const bool advanced = captureTime > newest + PoseSampleTimeTolerance &&
                              pose->root.hasPosition &&
                              pose->root.worldPosition != _buffer.GetNewest().root.worldPosition;
        result.status = advanced ? PoseSampleStatus::Extrapolated : PoseSampleStatus::Held;
        if (advanced)
        {
            ++_stats.samplesExtrapolated;
        }
        else
        {
            ++_stats.samplesHeld;
        }
    }
    else
    {
        result.status = PoseSampleStatus::Sampled;
        ++_stats.samplesSampled;
    }

    // Report on the consumer's clock. SampleExtrapolated caps its own
    // timestamp at the lead limit, which would otherwise leak the buffer's
    // clock into a downstream timeline.
    pose->timestamp = evaluationTime;
    result.pose = std::move(pose);
    return result;
}

bool
LiveCaptureSource::GetTimeRange(double* startTime, double* endTime) const
{
    double oldest = 0.0;
    double newest = 0.0;
    if (!_buffer.GetTimeRange(&oldest, &newest))
    {
        return false;
    }
    if (startTime)
    {
        *startTime = oldest - _clockOffset;
    }
    if (endTime)
    {
        *endTime = newest - _clockOffset;
    }
    return true;
}

void
LiveCaptureSource::Reset()
{
    _buffer.Clear();
    _filter.Reset();
    _lastAccepted.reset();
    _observedJoints.reset();
}

} // namespace openstrata::motion
