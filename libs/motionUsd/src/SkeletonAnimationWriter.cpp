// SPDX-License-Identifier: Apache-2.0
#include "motionUsd/SkeletonAnimationWriter.h"

#include "motionUsd/MotionStage.h"

#include "pxr/base/gf/vec3h.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/usd/editContext.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/topology.h"
#include "pxr/usd/usdSkel/utils.h"

#include <cmath>
#include <set>

namespace openstrata::motion {
namespace {

bool
Fail(std::string* error, const char* detail)
{
    if (error) {
        *error = detail;
    }
    return false;
}

template <typename Vector>
bool
FiniteVector(const Vector& value)
{
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(static_cast<double>(value[axis]))) {
            return false;
        }
    }
    return true;
}

bool
CanAuthorAt(const pxr::UsdStagePtr& stage, pxr::SdfPath path)
{
    for (; !path.IsAbsoluteRootPath(); path = path.GetParentPath()) {
        const pxr::UsdPrim prim = stage->GetPrimAtPath(path);
        if (prim && (!prim.IsActive() || prim.IsAbstract() || prim.IsInstance() ||
                     prim.IsInstanceProxy() || prim.IsInPrototype())) {
            return false;
        }
    }
    return true;
}

} // namespace

bool
AuthorSkeletonAnimation(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeletonPath,
                        const pxr::SdfPath& animationPath, const pxr::VtTokenArray& joints,
                        const std::vector<SkeletonAnimationSample>& samples, std::string* error)
{
    if (!stage) {
        return Fail(error, "no stage to author into");
    }
    if (!skeletonPath.IsAbsolutePath() || !skeletonPath.IsPrimPath() ||
        skeletonPath.ContainsPrimVariantSelection() || !animationPath.IsAbsolutePath() ||
        !animationPath.IsPrimPath() || animationPath.ContainsPrimVariantSelection()) {
        return Fail(error, "skeleton and animation paths must be absolute prim paths");
    }
    if (animationPath.HasPrefix(skeletonPath) || skeletonPath.HasPrefix(animationPath)) {
        return Fail(error, "skeleton and animation paths overlap");
    }
    if (!CanAuthorAt(stage, skeletonPath) || !CanAuthorAt(stage, animationPath)) {
        return Fail(error, "cannot author through an inactive, abstract or instanced prim");
    }
    const pxr::SdfLayerHandle root = stage->GetRootLayer();
    if (!root->PermissionToEdit()) {
        return Fail(error, "the root layer cannot be edited");
    }
    if (stage->GetTimeCodesPerSecond() != MotionStageTimeCodesPerSecond) {
        return Fail(error, "the target stage must use 30 time codes per second");
    }
    if (stage->GetPrimAtPath(animationPath) || root->GetPrimAtPath(animationPath)) {
        return Fail(error, "the animation path already exists");
    }

    const pxr::UsdSkelSkeleton skeleton(stage->GetPrimAtPath(skeletonPath));
    pxr::VtTokenArray targetJoints;
    std::string reason;
    if (!skeleton || !skeleton.GetJointsAttr().Get(&targetJoints) || joints.empty() ||
        targetJoints != joints || !pxr::UsdSkelTopology(joints).Validate(&reason) ||
        std::set<pxr::TfToken>(joints.begin(), joints.end()).size() != joints.size()) {
        return Fail(error, "joints must match a valid target skeleton in its joint order");
    }
    pxr::VtMatrix4dArray rests;
    if (!skeleton.GetRestTransformsAttr().Get(&rests) || rests.size() != joints.size()) {
        return Fail(error, "the target must carry one rest transform per joint");
    }
    pxr::VtVec3hArray scales(joints.size());
    for (std::size_t joint = 0; joint < joints.size(); ++joint) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (!std::isfinite(rests[joint][row][column])) {
                    return Fail(error, "a rest transform is not finite");
                }
            }
        }
        pxr::GfVec3f translation;
        pxr::GfQuatf rotation;
        if (!pxr::UsdSkelDecomposeTransform(
                rests[joint], &translation, &rotation, &scales[joint]) ||
            !FiniteVector(scales[joint])) {
            return Fail(error, "a rest transform cannot provide a finite scale");
        }
    }
    if (samples.empty()) {
        return Fail(error, "the animation has no sample");
    }
    std::vector<double> timeCodes;
    for (const SkeletonAnimationSample& sample : samples) {
        const double timeCode = sample.timestamp * MotionStageTimeCodesPerSecond;
        const double frame = std::round(timeCode);
        const double snapped = std::fabs(timeCode - frame) <= 1e-6 ? frame : timeCode;
        if (!std::isfinite(sample.timestamp) || !std::isfinite(snapped) ||
            (!timeCodes.empty() && snapped <= timeCodes.back())) {
            return Fail(error, "sample time codes must be finite and strictly increasing");
        }
        if (sample.rotations.size() != joints.size() ||
            sample.translations.size() != joints.size()) {
            return Fail(error, "each sample must carry one rotation and translation per joint");
        }
        for (std::size_t joint = 0; joint < joints.size(); ++joint) {
            const auto& rotation = sample.rotations[joint];
            if (!FiniteVector(sample.translations[joint]) || !std::isfinite(rotation.GetReal()) ||
                !FiniteVector(rotation.GetImaginary()) ||
                std::fabs(static_cast<double>(rotation.GetLength()) - 1.0) > 1e-4) {
                return Fail(error, "samples must carry finite translations and unit quaternions");
            }
        }
        timeCodes.push_back(snapped);
    }

    // Validate before mutation, and restore root content if USD refuses any
    // authoring operation. No save happens here. The original edit target is
    // restored by UsdEditContext, including when the caller was editing an asset.
    const pxr::SdfLayerRefPtr backup = pxr::SdfLayer::CreateAnonymous();
    backup->TransferContent(root);
    const pxr::UsdEditContext edit(stage, root);
    const pxr::UsdSkelAnimation animation = pxr::UsdSkelAnimation::Define(stage, animationPath);
    bool authored = animation && animation.CreateJointsAttr().Set(joints) &&
                    animation.CreateScalesAttr().Set(scales);
    if (authored) {
        const pxr::UsdAttribute rotations = animation.CreateRotationsAttr();
        const pxr::UsdAttribute translations = animation.CreateTranslationsAttr();
        for (std::size_t sample = 0; sample < samples.size() && authored; ++sample) {
            const pxr::UsdTimeCode time(timeCodes[sample]);
            authored = rotations.Set(samples[sample].rotations, time) &&
                       translations.Set(samples[sample].translations, time);
        }
    }
    if (authored) {
        const pxr::UsdPrim target = stage->OverridePrim(skeletonPath);
        const pxr::UsdSkelBindingAPI binding = pxr::UsdSkelBindingAPI::Apply(target);
        authored = binding && binding.CreateAnimationSourceRel().SetTargets({animationPath});
        // A stronger session opinion can make a successful root-layer write
        // ineffective. Refuse rather than report a bake the rig never uses.
        std::vector<pxr::SdfPath> composedTargets;
        authored = authored && binding.GetAnimationSourceRel().GetTargets(&composedTargets) &&
                   composedTargets == std::vector<pxr::SdfPath>({animationPath});
    }
    if (!authored) {
        root->TransferContent(backup);
        return Fail(error, "could not author the animation and skeleton binding");
    }
    if (error) {
        error->clear();
    }
    return true;
}

} // namespace openstrata::motion
