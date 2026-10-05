// SPDX-License-Identifier: Apache-2.0
#include "motionUsd/SkeletonAnimationWriter.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/usd/sdf/primSpec.h"
#include "pxr/usd/usd/references.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/cache.h"
#include "pxr/usd/usdSkel/root.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/skeletonQuery.h"
#include "pxr/usd/usdSkel/utils.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using openstrata::motion::AuthorSkeletonAnimation;
using openstrata::motion::SkeletonAnimationSample;

namespace {

const pxr::SdfPath kSkeleton("/World/Character/Skeleton");
const pxr::SdfPath kAnimation("/World/Motions/Baked");

std::string
Content(const pxr::SdfLayerHandle& layer)
{
    std::string result;
    assert(layer->ExportToString(&result));
    return result;
}

struct Fixture {
    pxr::UsdStageRefPtr avatar = pxr::UsdStage::CreateInMemory();
    pxr::UsdStageRefPtr source = pxr::UsdStage::CreateInMemory();
    pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    pxr::VtTokenArray joints{pxr::TfToken("base"), pxr::TfToken("base/tip")};
    pxr::VtMatrix4dArray rests;
    std::vector<SkeletonAnimationSample> samples;

    Fixture()
    {
        const auto asset = pxr::UsdSkelRoot::Define(avatar, pxr::SdfPath("/Asset"));
        avatar->SetDefaultPrim(asset.GetPrim());
        const auto skeleton = pxr::UsdSkelSkeleton::Define(avatar, pxr::SdfPath("/Asset/Skeleton"));
        assert(skeleton.CreateJointsAttr().Set(joints));
        const pxr::VtVec3fArray translations{pxr::GfVec3f(10, 0, 0), pxr::GfVec3f(0, 2, 0)};
        const pxr::VtQuatfArray rotations(2, pxr::GfQuatf(1.0f));
        const pxr::VtVec3hArray scales{pxr::GfVec3h(1.0f), pxr::GfVec3h(1.5f)};
        assert(pxr::UsdSkelMakeTransforms(translations, rotations, scales, &rests));
        assert(skeleton.CreateRestTransformsAttr().Set(rests));
        pxr::VtMatrix4dArray binds = rests;
        binds[1] = rests[1] * rests[0];
        assert(skeleton.CreateBindTransformsAttr().Set(binds));
        const auto idle = pxr::UsdSkelAnimation::Define(avatar, pxr::SdfPath("/Asset/Idle"));
        assert(idle.CreateJointsAttr().Set(joints));
        assert(idle.CreateTranslationsAttr().Set(translations));
        assert(idle.CreateRotationsAttr().Set(rotations));
        assert(idle.CreateScalesAttr().Set(scales));
        assert(pxr::UsdSkelBindingAPI::Apply(skeleton.GetPrim())
                   .CreateAnimationSourceRel()
                   .SetTargets({idle.GetPath()}));

        const auto sourceRoot = pxr::UsdSkelRoot::Define(source, pxr::SdfPath("/Animation"));
        source->SetDefaultPrim(sourceRoot.GetPrim());
        stage->SetTimeCodesPerSecond(30.0);
        stage->SetStartTimeCode(-10.0);
        stage->SetEndTimeCode(100.0);
        pxr::UsdGeomSetStageMetersPerUnit(stage, 0.01);
        assert(stage->DefinePrim(pxr::SdfPath("/World/Character"))
                   .GetReferences()
                   .AddReference(avatar->GetRootLayer()->GetIdentifier()));
        assert(stage->DefinePrim(pxr::SdfPath("/World/Motions/Source"))
                   .GetReferences()
                   .AddReference(source->GetRootLayer()->GetIdentifier()));
        samples.push_back({0.0, rotations, translations});
        samples.push_back({1.0 / 60.0, rotations, translations});
        samples.push_back({1.0 / 30.0, rotations, translations});
        samples.back().rotations[1] =
            pxr::GfQuatf(pxr::GfRotation(pxr::GfVec3d(1, 0, 0), 45.0).GetQuat());
        samples.back().translations[0] = pxr::GfVec3f(12, 0, 3);
    }
};

void
TestAReferencedRigResolvesTheBakedTransforms()
{
    Fixture fixture;
    const std::string avatarBefore = Content(fixture.avatar->GetRootLayer());
    const std::string sourceBefore = Content(fixture.source->GetRootLayer());
    // Even when a caller is editing the referenced asset, this API writes to
    // the derivative root and restores that edit target afterwards.
    fixture.stage->SetEditTarget(fixture.avatar->GetRootLayer());
    const auto oldEditTarget = fixture.stage->GetEditTarget();
    std::string error = "old error";
    assert(AuthorSkeletonAnimation(
        fixture.stage, kSkeleton, kAnimation, fixture.joints, fixture.samples, &error));
    assert(error.empty());
    assert(fixture.stage->GetEditTarget() == oldEditTarget);
    assert(Content(fixture.avatar->GetRootLayer()) == avatarBefore);
    assert(Content(fixture.source->GetRootLayer()) == sourceBefore);
    assert(fixture.stage->GetStartTimeCode() == -10.0);
    assert(fixture.stage->GetEndTimeCode() == 100.0);
    assert(pxr::UsdGeomGetStageMetersPerUnit(fixture.stage) == 0.01);

    const auto override = fixture.stage->GetRootLayer()->GetPrimAtPath(kSkeleton);
    assert(override && override->GetSpecifier() == pxr::SdfSpecifierOver);
    assert(override->GetTypeName().IsEmpty());
    assert(!fixture.stage->GetRootLayer()->GetAttributeAtPath(
        kSkeleton.AppendProperty(pxr::TfToken("restTransforms"))));
    assert(!fixture.stage->GetRootLayer()->GetAttributeAtPath(
        kSkeleton.AppendProperty(pxr::TfToken("joints"))));

    // Reopen from serialized derivative content, preserving the anonymous
    // reference identifiers, to check composition rather than writer handles.
    const auto reopenedLayer = pxr::SdfLayer::CreateAnonymous("bake.usda");
    assert(reopenedLayer->ImportFromString(Content(fixture.stage->GetRootLayer())));
    const auto reopened = pxr::UsdStage::Open(reopenedLayer);
    const pxr::UsdSkelSkeleton skeleton(reopened->GetPrimAtPath(kSkeleton));
    const pxr::UsdSkelAnimation animation(reopened->GetPrimAtPath(kAnimation));
    pxr::VtVec3hArray scales;
    assert(animation.GetScalesAttr().Get(&scales));
    assert(scales == pxr::VtVec3hArray({pxr::GfVec3h(1.0f), pxr::GfVec3h(1.5f)}));
    assert(animation.GetScalesAttr().GetNumTimeSamples() == 0);
    std::vector<double> times;
    assert(animation.GetRotationsAttr().GetTimeSamples(&times));
    assert(times == std::vector<double>({0.0, 0.5, 1.0}));
    assert(animation.GetTranslationsAttr().GetTimeSamples(&times));
    assert(times == std::vector<double>({0.0, 0.5, 1.0}));
    pxr::UsdSkelCache cache;
    const auto query = cache.GetSkelQuery(skeleton);
    assert(query && query.GetAnimQuery());
    assert(query.GetAnimQuery().GetPrim().GetPath() == kAnimation);
    for (std::size_t sample = 0; sample < fixture.samples.size(); ++sample) {
        pxr::VtMatrix4dArray expected;
        assert(pxr::UsdSkelMakeTransforms(fixture.samples[sample].translations,
                                          fixture.samples[sample].rotations,
                                          scales,
                                          &expected));
        pxr::VtMatrix4dArray resolved;
        assert(query.ComputeJointLocalTransforms(&resolved, pxr::UsdTimeCode(times[sample])));
        assert(resolved.size() == expected.size());
        for (std::size_t joint = 0; joint < expected.size(); ++joint) {
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 4; ++column) {
                    assert(std::fabs(resolved[joint][row][column] - expected[joint][row][column]) <
                           1e-5);
                }
            }
        }
    }
}

void
Refused(Fixture& fixture, const pxr::SdfPath& skeleton = kSkeleton,
        const pxr::SdfPath& animation = kAnimation)
{
    const std::string before = Content(fixture.stage->GetRootLayer());
    const std::string avatarBefore = Content(fixture.avatar->GetRootLayer());
    std::string error;
    assert(!AuthorSkeletonAnimation(
        fixture.stage, skeleton, animation, fixture.joints, fixture.samples, &error));
    assert(!error.empty());
    assert(Content(fixture.stage->GetRootLayer()) == before);
    assert(Content(fixture.avatar->GetRootLayer()) == avatarBefore);
}

void
TestInvalidInputsLeaveTheDerivativeUntouched()
{
    assert(!AuthorSkeletonAnimation({}, kSkeleton, kAnimation, {}, {}, nullptr));
    Fixture fixture;
    Refused(fixture, pxr::SdfPath("relative"));
    Refused(fixture, pxr::SdfPath("/Missing"));
    Refused(fixture, kSkeleton, pxr::SdfPath("/World"));
    Refused(fixture, kSkeleton, kSkeleton.AppendChild(pxr::TfToken("Animation")));
    Refused(fixture, kSkeleton, pxr::SdfPath("/World{variant=value}/Baked"));
    Refused(fixture, kSkeleton, pxr::SdfPath("/World/Motions/Source"));
    fixture.stage->SetTimeCodesPerSecond(24.0);
    Refused(fixture);
    fixture.stage->SetTimeCodesPerSecond(30.0);
    fixture.stage->GetRootLayer()->SetPermissionToEdit(false);
    Refused(fixture);
    fixture.stage->GetRootLayer()->SetPermissionToEdit(true);

    const auto samples = fixture.samples;
    fixture.samples.clear();
    Refused(fixture);
    fixture.samples = samples;
    fixture.samples[1].rotations.pop_back();
    Refused(fixture);
    fixture.samples = samples;
    fixture.samples[1].translations.pop_back();
    Refused(fixture);
    fixture.samples = samples;
    fixture.samples[1].timestamp = 0.0;
    Refused(fixture);
    fixture.samples[1].timestamp = -1.0;
    Refused(fixture);
    fixture.samples[1].timestamp = 1e-10; // collapses onto the first frame
    Refused(fixture);
    fixture.samples[1].timestamp = std::numeric_limits<double>::infinity();
    Refused(fixture);
    fixture.samples[1].timestamp = std::numeric_limits<double>::quiet_NaN();
    Refused(fixture);
    fixture.samples[1].timestamp = std::numeric_limits<double>::max();
    Refused(fixture);
    fixture.samples = samples;
    fixture.samples[0].rotations[0] = pxr::GfQuatf(0.0f);
    Refused(fixture);
    fixture.samples[0].rotations[0] = pxr::GfQuatf(std::numeric_limits<float>::quiet_NaN());
    Refused(fixture);
    fixture.samples = samples;
    fixture.samples[0].translations[0][2] = std::numeric_limits<float>::infinity();
    Refused(fixture);
    fixture.samples = samples;
    std::swap(fixture.joints[0], fixture.joints[1]);
    Refused(fixture);

    Fixture invalidRest;
    pxr::UsdSkelSkeleton(invalidRest.avatar->GetPrimAtPath(pxr::SdfPath("/Asset/Skeleton")))
        .GetRestTransformsAttr()
        .Set(pxr::VtMatrix4dArray());
    Refused(invalidRest);
    Fixture instanced;
    instanced.stage->GetPrimAtPath(pxr::SdfPath("/World/Character")).SetInstanceable(true);
    Refused(instanced);

    Fixture strongerBinding;
    strongerBinding.stage->SetEditTarget(strongerBinding.stage->GetSessionLayer());
    assert(pxr::UsdSkelBindingAPI::Apply(strongerBinding.stage->GetPrimAtPath(kSkeleton))
               .CreateAnimationSourceRel()
               .SetTargets({pxr::SdfPath("/World/Character/Idle")}));
    const std::string sessionBefore = Content(strongerBinding.stage->GetSessionLayer());
    const auto editBefore = strongerBinding.stage->GetEditTarget();
    Refused(strongerBinding); // exercises rollback after animation authoring
    assert(Content(strongerBinding.stage->GetSessionLayer()) == sessionBefore);
    assert(strongerBinding.stage->GetEditTarget() == editBefore);

    Fixture nonFiniteRest;
    nonFiniteRest.rests[0][0][0] = std::numeric_limits<double>::infinity();
    assert(
        pxr::UsdSkelSkeleton(nonFiniteRest.avatar->GetPrimAtPath(pxr::SdfPath("/Asset/Skeleton")))
            .GetRestTransformsAttr()
            .Set(nonFiniteRest.rests));
    Refused(nonFiniteRest);

    Fixture existing;
    assert(AuthorSkeletonAnimation(
        existing.stage, kSkeleton, kAnimation, existing.joints, existing.samples, nullptr));
    Refused(existing); // replacing a prior bake must not leave stale samples
}

} // namespace

int
main()
{
    TestAReferencedRigResolvesTheBakedTransforms();
    TestInvalidInputsLeaveTheDerivativeUntouched();
    std::puts("motionUsd skeleton animation writer tests passed");
    return 0;
}
