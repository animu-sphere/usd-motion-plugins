// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionUsd/ClipReader.h"
#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/vec3d.h"

namespace openstrata::motion {

// Default-time, owned skeleton values for motion configuration. Rest matrices
// are parent-local, with translations in metres. Placement is skeleton-to-world
// in metres; it is deliberately separate from the rest used by a retargeter.
// No stage or prim handle is retained. ReadMotionSkeleton builds typed values
// from these arrays using motionRetarget's existing owner builders.
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

enum class SkeletonReadRole {
    Generic,
    SemanticSource,
};

struct MotionSkeletonMetadata {
    std::string skeletonPath;
    double metersPerUnit = 1;
    pxr::GfVec3d worldTranslation = pxr::GfVec3d(0);
    pxr::GfQuatd worldRotation = pxr::GfQuatd(1);
};

struct MotionSkeletonRead {
    SkeletonDescriptor skeleton;
    // Present exactly for SemanticSource. Generic rigs are not assigned human
    // roles from their names. Source extraction uses BuildSourceRestPose.
    std::optional<SourceRestPose> sourceRest;
    MotionSkeletonMetadata metadata;
};

// Strict Y-up, caller-asserted canonical forward basis, authored rest profile.
// Rejects malformed tokens/topology, nonfinite/float-unrepresentable rest,
// reflection, zero scale, shear (>1e-6), perspective (>1e-12) and nonrigid
// placement. Tiny affine residuals are canonicalized in the returned copy.
// A refusal leaves read unchanged and returns one owner code/subject/detail.
// Success clears diagnostic. Null read is refused; diagnostic may be null.
MOTIONUSD_API bool ReadSkeleton(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
                                SkeletonStageRead* read, SkeletonReadDiagnostic* diagnostic);

// The same strict default-time profile, with a descriptor built and validated
// by motionRetarget. The caller explicitly selects generic or semantic-source
// interpretation; a semantic source must name at least one HumanJoint and
// cannot name the same bone twice. Placement stays separate from local rest.
// Failure leaves read unchanged; success clears diagnostic (which may be null).
MOTIONUSD_API bool ReadMotionSkeleton(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
                                      SkeletonReadRole role, MotionSkeletonRead* read,
                                      SkeletonReadDiagnostic* diagnostic);

// ReadMotionStage with the strict skeleton profile and canonical clip-space
// requirements: metre units, identity skeleton world placement and positive
// finite encoding rate. Returns compatible authored arrays plus owner-built
// descriptor/sourceRest together. Metadata/warnings are kept.
// The permissive ReadMotionStage contract is unchanged. Refusal leaves read
// unchanged; common clip-reader failures are identified by MOTION_USD_READ;
// skeleton/source-rest failures retain their owner diagnostic.
MOTIONUSD_API bool ReadCanonicalMotionStage(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
                                            MotionStageRead* read,
                                            SkeletonReadDiagnostic* diagnostic);

// The same strict profile with explicit owner-selected scalar/gaze inputs.
// Input interpretation stays in ReadMotionStage; placement remains host work.
MOTIONUSD_API bool ReadCanonicalMotionStage(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
                                            const MotionStageReadOptions& options,
                                            MotionStageRead* read,
                                            SkeletonReadDiagnostic* diagnostic);

} // namespace openstrata::motion
