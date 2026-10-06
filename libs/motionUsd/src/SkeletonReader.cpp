// SPDX-License-Identifier: Apache-2.0
#include "motionUsd/SkeletonReader.h"

#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/xformCache.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/topology.h"

#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace openstrata::motion {
namespace {
bool
Fail(SkeletonReadDiagnostic* diagnostic, const char* code, const std::string& subject,
     const char* detail)
{
    if (diagnostic)
        *diagnostic = {code, subject, detail};
    return false;
}

bool
ValidateMatrix(pxr::GfMatrix4d& matrix, const std::string& subject, bool rigid,
               SkeletonReadDiagnostic* diagnostic)
{
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            if (!std::isfinite(matrix[row][column]))
                return Fail(diagnostic, "MOTION_USD_NONFINITE", subject, "matrix is not finite");
    for (int row = 0; row < 4; ++row) {
        const double expected = row == 3 ? 1.0 : 0.0;
        if (std::abs(matrix[row][3] - expected) > 1e-12)
            return Fail(diagnostic, "MOTION_USD_NONAFFINE", subject, "perspective is unsupported");
        matrix[row][3] = expected;
    }
    pxr::GfVec3d rows[3];
    for (int row = 0; row < 3; ++row) {
        rows[row] = pxr::GfVec3d(matrix[row][0], matrix[row][1], matrix[row][2]);
        const double length = rows[row].GetLength();
        if (!std::isfinite(length) || length <= 0 || length > std::numeric_limits<float>::max() ||
            (!rigid && static_cast<float>(length) == 0))
            return Fail(
                diagnostic, "MOTION_USD_SCALE", subject, "scale is not positive and representable");
        if (rigid && std::abs(length - 1) > 1e-6)
            return Fail(
                diagnostic, "MOTION_USD_PLACEMENT_SCALE", subject, "placement must be rigid");
        rows[row] /= length;
    }
    for (int row = 0; row < 3; ++row)
        for (int column = row + 1; column < 3; ++column)
            if (std::abs(pxr::GfDot(rows[row], rows[column])) > 1e-6)
                return Fail(diagnostic, "MOTION_USD_SHEAR", subject, "shear is unsupported");
    // Use normalized rows so determinant overflow/underflow cannot mask a
    // reflection on otherwise representable scales.
    if (pxr::GfDot(pxr::GfCross(rows[0], rows[1]), rows[2]) <= 0)
        return Fail(diagnostic, "MOTION_USD_REFLECTION", subject, "reflection is unsupported");
    return true;
}
} // namespace

bool
ReadSkeleton(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path, SkeletonStageRead* read,
             SkeletonReadDiagnostic* diagnostic)
{
    const auto subject = path.GetString();
    if (!read)
        return Fail(diagnostic, "MOTION_USD_OUTPUT", subject, "null output");
    if (!stage)
        return Fail(diagnostic, "MOTION_USD_STAGE", subject, "null stage");
    if (!path.IsAbsolutePath() || !path.IsPrimPath() || path.ContainsPrimVariantSelection())
        return Fail(
            diagnostic, "MOTION_USD_SKELETON_PATH", subject, "expected an absolute prim path");
    const pxr::UsdSkelSkeleton skeleton(stage->GetPrimAtPath(path));
    if (!skeleton)
        return Fail(diagnostic, "MOTION_USD_SKELETON", subject, "not a skeleton");
    if (pxr::UsdGeomGetStageUpAxis(stage) != pxr::UsdGeomTokens->y)
        return Fail(diagnostic, "MOTION_USD_UP_AXIS", subject, "expected Y-up");
    SkeletonStageRead result;
    result.metersPerUnit = pxr::UsdGeomGetStageMetersPerUnit(stage);
    if (!std::isfinite(result.metersPerUnit) || result.metersPerUnit <= 0)
        return Fail(diagnostic, "MOTION_USD_UNITS", subject, "expected positive finite units");
    pxr::VtTokenArray tokens;
    pxr::VtMatrix4dArray rests;
    if (!skeleton.GetJointsAttr().Get(&tokens) || tokens.empty() ||
        tokens.size() > size_t(std::numeric_limits<int>::max()))
        return Fail(diagnostic,
                    "MOTION_USD_JOINTS",
                    subject,
                    "expected a nonempty representable joint array");
    if (!skeleton.GetRestTransformsAttr().Get(&rests) || rests.size() != tokens.size())
        return Fail(
            diagnostic, "MOTION_USD_REST_COUNT", subject, "expected one authored rest per joint");
    std::set<std::string> unique;
    std::map<std::string, int> indices;
    for (size_t joint = 0; joint < tokens.size(); ++joint) {
        const auto token = tokens[joint].GetString();
        const pxr::SdfPath jointPath(token);
        if (token.empty() || !jointPath.IsPrimPath() || jointPath.IsAbsolutePath() ||
            jointPath.ContainsPrimVariantSelection() || !unique.insert(token).second)
            return Fail(diagnostic,
                        "MOTION_USD_JOINT_TOKEN",
                        token,
                        "expected a unique relative joint path");
        if (!ValidateMatrix(rests[joint], token, false, diagnostic))
            return false;
        for (int axis = 0; axis < 3; ++axis) {
            rests[joint][3][axis] *= result.metersPerUnit;
            if (!std::isfinite(rests[joint][3][axis]) ||
                std::abs(rests[joint][3][axis]) > std::numeric_limits<float>::max())
                return Fail(diagnostic,
                            "MOTION_USD_FLOAT_RANGE",
                            token,
                            "rest translation exceeds float range");
        }
        result.skeleton.jointTokens.push_back(token);
        result.skeleton.restTransforms.push_back(rests[joint]);
        indices.emplace(token, static_cast<int>(joint));
    }
    const pxr::UsdSkelTopology topology(tokens);
    std::string reason;
    if (!topology.Validate(&reason)) {
        if (diagnostic)
            *diagnostic = {"MOTION_USD_TOPOLOGY", subject, reason};
        return false;
    }
    for (size_t joint = 0; joint < tokens.size(); ++joint) {
        // Match the descriptor's immediate-parent convention. A missing parent
        // is a root; silently choosing a more distant ancestor would change
        // the meaning of the authored parent-local rest.
        const auto parent =
            indices.find(pxr::SdfPath(tokens[joint].GetString()).GetParentPath().GetString());
        const int expected = parent == indices.end() ? -1 : parent->second;
        if (topology.GetParent(joint) != expected)
            return Fail(diagnostic,
                        "MOTION_USD_PARENT_MAPPING",
                        tokens[joint].GetString(),
                        "topology and immediate-parent rest mapping disagree");
        result.parents.push_back(topology.GetParent(joint));
    }
    pxr::UsdGeomXformCache cache;
    auto world = cache.GetLocalToWorldTransform(skeleton.GetPrim());
    if (!ValidateMatrix(world, subject, true, diagnostic))
        return false;
    result.worldRotation = world.ExtractRotationQuat().GetNormalized();
    for (int axis = 0; axis < 3; ++axis) {
        result.worldTranslation[axis] = world[3][axis] * result.metersPerUnit;
        if (!std::isfinite(result.worldTranslation[axis]))
            return Fail(diagnostic,
                        "MOTION_USD_PLACEMENT_RANGE",
                        subject,
                        "placement translation is not finite");
    }
    result.skeleton.path = subject;
    result.skeleton.restTransformsAuthored = true;
    *read = std::move(result);
    if (diagnostic)
        *diagnostic = {};
    return true;
}

bool
ReadCanonicalMotionStage(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
                         MotionStageRead* read, SkeletonReadDiagnostic* diagnostic)
{
    return ReadCanonicalMotionStage(stage, path, MotionStageReadOptions{}, read, diagnostic);
}

bool
ReadCanonicalMotionStage(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
                         const MotionStageReadOptions& options, MotionStageRead* read,
                         SkeletonReadDiagnostic* diagnostic)
{
    const auto subject = path.GetString();
    if (!read)
        return Fail(diagnostic, "MOTION_USD_OUTPUT", subject, "null output");
    SkeletonStageRead skeleton;
    if (!ReadSkeleton(stage, path, &skeleton, diagnostic))
        return false;
    if (skeleton.metersPerUnit != 1)
        return Fail(diagnostic, "MOTION_USD_UNITS", subject, "clip samples require metre units");
    if (!std::isfinite(stage->GetTimeCodesPerSecond()) || stage->GetTimeCodesPerSecond() <= 0)
        return Fail(
            diagnostic, "MOTION_USD_RATE", subject, "expected positive finite encoding rate");
    for (int axis = 0; axis < 3; ++axis)
        if (std::abs(skeleton.worldTranslation[axis]) > 1e-12 ||
            std::abs(skeleton.worldRotation.GetImaginary()[axis]) > 1e-12)
            return Fail(diagnostic,
                        "MOTION_USD_PLACEMENT",
                        subject,
                        "clip samples require identity placement");
    if (std::abs(std::abs(skeleton.worldRotation.GetReal()) - 1) > 1e-12)
        return Fail(
            diagnostic, "MOTION_USD_PLACEMENT", subject, "clip samples require identity placement");
    MotionStageRead result;
    std::string error;
    if (!ReadMotionStage(stage, subject, options, &result, &error)) {
        if (diagnostic)
            *diagnostic = {"MOTION_USD_READ", subject, error};
        return false;
    }
    result.skeleton = std::move(skeleton.skeleton);
    *read = std::move(result);
    if (diagnostic)
        *diagnostic = {};
    return true;
}
} // namespace openstrata::motion
