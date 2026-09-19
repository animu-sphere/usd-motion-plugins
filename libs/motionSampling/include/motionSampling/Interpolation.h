// SPDX-License-Identifier: Apache-2.0
//
// Pose interpolation. Like motionCore this is a value-only contract: no USD
// stage, plug, file-format, network, or vendor SDK API is allowed here.
#pragma once

#include "motionSampling/api.h"

#include "motionCore/MotionPose.h"

#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec3f.h"

namespace openstrata::motion
{

// Shortest-arc interpolation between two unit quaternions. `t` is clamped to
// [0, 1]; near-antipodal inputs take the short path, so a clip authored with a
// sign-flipped quaternion does not spin the long way round.
MOTIONSAMPLING_API pxr::GfQuatf SlerpShortest(const pxr::GfQuatf& a, const pxr::GfQuatf& b, float t);

// Component-wise interpolation. A presence flag survives only where both
// endpoints carry the component; where exactly one does, that endpoint's value
// is held rather than faded toward zero, because a missing sample is not a
// zero-valued sample (motion contract, `motionCore` value contract).
MOTIONSAMPLING_API RootMotion LerpRootMotion(const RootMotion& a, const RootMotion& b, float t);

// Interpolates two poses joint by joint. A joint valid in both endpoints is
// slerped; a joint valid in exactly one is copied from that endpoint; a joint
// valid in neither stays absent. The result's timestamp is interpolated.
//
// Optional fields (confidence, channels, contacts) follow the same
// hold-not-fade rule: confidence is interpolated only where both endpoints
// carry it, channels are interpolated per name and a name only one endpoint
// reports is held at that weight rather than faded toward zero, and
// contacts are taken from the nearer endpoint because they are discrete. The
// metadata, which every pose carries, is the nearer endpoint's.
MOTIONSAMPLING_API MotionPose LerpPose(const MotionPose& a, const MotionPose& b, float t);

} // namespace openstrata::motion
