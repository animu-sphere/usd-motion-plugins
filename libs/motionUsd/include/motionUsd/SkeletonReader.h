// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionUsd/ClipReader.h"
#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/vec3d.h"

namespace openstrata::motion {

// Default-time, owned skeleton values for motion configuration. Rest matrices
// are parent-local, with translations in metres. Placement is skeleton-to-world
// in metres; it is deliberately separate from the rest used by a retargeter.
// No stage or prim handle is retained. Feed the arrays to motionRetarget's
// BuildSkeletonDescriptor; motionUsd itself has no retarget dependency.
struct SkeletonStageRead {
    MotionStageSkeleton skeleton;
    std::vector<int> parents;
    pxr::GfVec3d worldTranslation = pxr::GfVec3d(0);
    pxr::GfQuatd worldRotation = pxr::GfQuatd(1);
    double metersPerUnit = 1;
};

struct SkeletonReadDiagnostic {
    std::string code;
    std::string subject;
    std::string detail;
};

// Strict Y-up, caller-asserted canonical forward basis, authored rest profile.
// Rejects malformed tokens/topology, nonfinite/float-unrepresentable rest,
// reflection, zero scale, shear (>1e-6), perspective (>1e-12) and nonrigid
// placement. Tiny affine residuals are canonicalized in the returned copy.
// A refusal leaves read unchanged and returns one owner code/subject/detail.
// Success clears diagnostic. Null read is refused; diagnostic may be null.
MOTIONUSD_API bool ReadSkeleton(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
                                SkeletonStageRead* read, SkeletonReadDiagnostic* diagnostic);

// ReadMotionStage with the strict skeleton profile and canonical clip-space
// requirements: metre units, identity skeleton world placement and positive
// finite encoding rate. The returned authored rest is validated, ready for the
// motionRetarget descriptor/source-rest builders. Metadata/warnings are kept.
// The permissive ReadMotionStage contract is unchanged. Refusal leaves read
// unchanged; any reader failure is identified by MOTION_USD_READ.
MOTIONUSD_API bool ReadCanonicalMotionStage(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
                                            MotionStageRead* read,
                                            SkeletonReadDiagnostic* diagnostic);

} // namespace openstrata::motion
