// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionUsd/api.h"

#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/vt/array.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/stage.h"

#include <string>
#include <vector>

namespace openstrata::motion {

// Already evaluated joint-local values, in the target skeleton's joint order.
// Translations are in the target stage's units and basis; the caller converts
// canonical metre values when the stage uses another unit. No retargeting or
// avatar-format interpretation happens here (USD_MAPPING.md §6).
struct SkeletonAnimationSample {
    double timestamp = 0.0; // seconds
    pxr::VtQuatfArray rotations;
    pxr::VtVec3fArray translations;
};

// Authors a new UsdSkelAnimation and an animation-source binding override on
// an existing composed UsdSkelSkeleton, into the stage's root layer. A caller
// composes the avatar by reference first and saves the derivative afterwards.
// The referenced layers, rig attributes, stage metadata and current edit
// target are preserved. Constant scales come from the skeleton's rest.
//
// `joints` must exactly match the target's joints, and every sample must carry
// one finite translation and a unit quaternion per joint. Timestamps must be
// finite and strictly increasing even after conversion to time codes. The
// stage must use 30 time codes per second (USD_MAPPING.md §4.1).
//
// Refuses empty samples, missing/malformed rest transforms, existing animation
// paths, overlapping skeleton/animation paths, instance/prototype authoring,
// a root layer that cannot be edited, and a stronger binding opinion that
// prevents the new animation from binding. Refusal leaves root-layer content
// unchanged. `error` may be null, and is cleared on success.
MOTIONUSD_API bool
AuthorSkeletonAnimation(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeletonPath,
                        const pxr::SdfPath& animationPath, const pxr::VtTokenArray& joints,
                        const std::vector<SkeletonAnimationSample>& samples, std::string* error);

} // namespace openstrata::motion
