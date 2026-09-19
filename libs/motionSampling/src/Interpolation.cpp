// SPDX-License-Identifier: Apache-2.0
#include "motionSampling/Interpolation.h"

#include <algorithm>
#include <cmath>

namespace openstrata::motion
{
namespace
{

float
Clamp01(float t)
{
    return std::min(std::max(t, 0.0f), 1.0f);
}

pxr::GfVec3f
LerpVec3(const pxr::GfVec3f& a, const pxr::GfVec3f& b, float t)
{
    return a + (b - a) * t;
}

} // namespace

pxr::GfQuatf
SlerpShortest(const pxr::GfQuatf& a, const pxr::GfQuatf& b, float t)
{
    const float alpha = Clamp01(t);
    const pxr::GfQuatf from = a.GetNormalized();
    pxr::GfQuatf to = b.GetNormalized();

    // q and -q describe the same orientation, so choose the representative on
    // the same hemisphere as `from` before interpolating. GfSlerp is documented
    // to take the shortest arc, but doing it here keeps this function's
    // behavior independent of that and makes it directly testable.
    if (pxr::GfDot(from, to) < 0.0f)
    {
        to = pxr::GfQuatf(-to.GetReal(), -to.GetImaginary());
    }
    return pxr::GfSlerp(static_cast<double>(alpha), from, to).GetNormalized();
}

RootMotion
LerpRootMotion(const RootMotion& a, const RootMotion& b, float t)
{
    const float alpha = Clamp01(t);
    RootMotion result;

    result.hasPosition = a.hasPosition || b.hasPosition;
    if (a.hasPosition && b.hasPosition)
    {
        result.worldPosition = LerpVec3(a.worldPosition, b.worldPosition, alpha);
    }
    else if (a.hasPosition)
    {
        result.worldPosition = a.worldPosition;
    }
    else if (b.hasPosition)
    {
        result.worldPosition = b.worldPosition;
    }

    result.hasOrientation = a.hasOrientation || b.hasOrientation;
    if (a.hasOrientation && b.hasOrientation)
    {
        result.worldOrientation = SlerpShortest(a.worldOrientation, b.worldOrientation, alpha);
    }
    else if (a.hasOrientation)
    {
        result.worldOrientation = a.worldOrientation;
    }
    else if (b.hasOrientation)
    {
        result.worldOrientation = b.worldOrientation;
    }

    result.hasLinearVelocity = a.hasLinearVelocity || b.hasLinearVelocity;
    if (a.hasLinearVelocity && b.hasLinearVelocity)
    {
        result.linearVelocity = LerpVec3(a.linearVelocity, b.linearVelocity, alpha);
    }
    else if (a.hasLinearVelocity)
    {
        result.linearVelocity = a.linearVelocity;
    }
    else if (b.hasLinearVelocity)
    {
        result.linearVelocity = b.linearVelocity;
    }

    result.hasAngularVelocity = a.hasAngularVelocity || b.hasAngularVelocity;
    if (a.hasAngularVelocity && b.hasAngularVelocity)
    {
        result.angularVelocity = LerpVec3(a.angularVelocity, b.angularVelocity, alpha);
    }
    else if (a.hasAngularVelocity)
    {
        result.angularVelocity = a.angularVelocity;
    }
    else if (b.hasAngularVelocity)
    {
        result.angularVelocity = b.angularVelocity;
    }

    return result;
}

MotionPose
LerpPose(const MotionPose& a, const MotionPose& b, float t)
{
    const float alpha = Clamp01(t);
    MotionPose result;
    result.timestamp = a.timestamp + (b.timestamp - a.timestamp) * alpha;
    result.root = LerpRootMotion(a.root, b.root, alpha);

    for (std::size_t i = 0; i < HumanJointCount; ++i)
    {
        const bool inA = a.validRotations.test(i);
        const bool inB = b.validRotations.test(i);
        if (inA && inB)
        {
            result.localRotations[i] =
                SlerpShortest(a.localRotations[i], b.localRotations[i], alpha);
        }
        else if (inA)
        {
            result.localRotations[i] = a.localRotations[i];
        }
        else if (inB)
        {
            result.localRotations[i] = b.localRotations[i];
        }
        else
        {
            continue;
        }
        result.validRotations.set(i);
    }

    if (a.confidence && b.confidence)
    {
        std::array<float, HumanJointCount> blended{};
        for (std::size_t i = 0; i < HumanJointCount; ++i)
        {
            blended[i] = (*a.confidence)[i] + ((*b.confidence)[i] - (*a.confidence)[i]) * alpha;
        }
        result.confidence = blended;
    }
    else if (a.confidence)
    {
        result.confidence = a.confidence;
    }
    else if (b.confidence)
    {
        result.confidence = b.confidence;
    }

    // Channels follow the joints' rule rather than confidence's, because they
    // are keyed by name and the two endpoints need not carry the same names: a
    // weight reported by both is interpolated, one reported by a single
    // endpoint is held at that value, and a name neither reported stays absent.
    // Fading a one-sided weight toward zero would invent a channel closing that
    // no producer described -- the same reason a missing joint is held rather
    // than eased to identity.
    for (const MotionChannel& entry : a.channels.entries)
    {
        const float* other = b.channels.Find(entry.name);
        result.channels.Set(entry.name, other ? entry.value + (*other - entry.value) * alpha
                                                 : entry.value);
    }
    for (const MotionChannel& entry : b.channels.entries)
    {
        if (!a.channels.Find(entry.name))
        {
            result.channels.Set(entry.name, entry.value);
        }
    }

    // A look-at target follows the same rule again: interpolated when both
    // endpoints named one, held when a single endpoint did. It is a point in the
    // same space as the root position, so a straight lerp is the motion of the
    // thing being watched -- and easing a one-sided target toward the origin
    // would aim the gaze at a place no producer ever named.
    if (a.lookAtTarget && b.lookAtTarget)
    {
        result.lookAtTarget = LerpVec3(*a.lookAtTarget, *b.lookAtTarget, alpha);
    }
    else if (a.lookAtTarget)
    {
        result.lookAtTarget = a.lookAtTarget;
    }
    else if (b.lookAtTarget)
    {
        result.lookAtTarget = b.lookAtTarget;
    }

    // Contact state and provenance are discrete, so they snap to the nearer
    // endpoint instead of being averaged into a value neither side reported.
    const MotionPose& nearer = (alpha < 0.5f) ? a : b;
    const MotionPose& farther = (alpha < 0.5f) ? b : a;
    result.contacts = nearer.contacts ? nearer.contacts : farther.contacts;

    // Provenance is always present, so there is no absent side to fall back
    // from: the nearer endpoint's metadata is taken whole, its stamp and
    // sequence number included. An interpolated pose was never observed, so it
    // is not given a stamp or a counter of its own; it answers with those of
    // the observation it is closest to.
    result.metadata = nearer.metadata;

    return result;
}

} // namespace openstrata::motion
