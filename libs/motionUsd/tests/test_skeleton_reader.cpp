// SPDX-License-Identifier: Apache-2.0
#include "motionUsd/SkeletonReader.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <string>

namespace {
using namespace openstrata::motion;
const pxr::SdfPath path("/Rig/Skeleton");
pxr::UsdStageRefPtr
Stage()
{
    auto stage = pxr::UsdStage::CreateInMemory();
    assert(pxr::UsdGeomSetStageUpAxis(stage, pxr::UsdGeomTokens->y));
    assert(pxr::UsdGeomSetStageMetersPerUnit(stage, 1));
    stage->SetTimeCodesPerSecond(60);
    const auto skeleton = pxr::UsdSkelSkeleton::Define(stage, path);
    const pxr::VtTokenArray joints{
        pxr::TfToken("hips"), pxr::TfToken("hips/head"), pxr::TfToken("extra")};
    assert(skeleton.CreateJointsAttr().Set(joints));
    pxr::GfMatrix4d hip(1), head(1), extra(1);
    hip.SetTranslate(pxr::GfVec3d(0, 80, 0));
    head.SetRotate(pxr::GfRotation(pxr::GfVec3d(0, 1, 0), 20));
    head.SetTranslateOnly(pxr::GfVec3d(0, 50, 0));
    extra.SetScale(pxr::GfVec3d(0.5, 1, 2));
    assert(skeleton.CreateRestTransformsAttr().Set(pxr::VtMatrix4dArray{hip, head, extra}));
    const auto animation = pxr::UsdSkelAnimation::Define(stage, pxr::SdfPath("/Rig/Animation"));
    assert(pxr::UsdSkelBindingAPI::Apply(skeleton.GetPrim())
               .CreateAnimationSourceRel()
               .SetTargets({animation.GetPath()}));
    assert(animation.CreateJointsAttr().Set(joints));
    assert(animation.CreateRotationsAttr().Set(pxr::VtQuatfArray(3, pxr::GfQuatf(1)), 0));
    assert(animation.CreateRotationsAttr().Set(pxr::VtQuatfArray(3, pxr::GfQuatf(1)), 60));
    return stage;
}
void
Reject(const pxr::UsdStagePtr& stage, const char* code, pxr::SdfPath selected = path)
{
    SkeletonStageRead read;
    read.skeleton.path = "retained";
    read.worldTranslation = pxr::GfVec3d(3, 4, 5);
    SkeletonReadDiagnostic diagnostic;
    assert(!ReadSkeleton(stage, selected, &read, &diagnostic));
    assert(diagnostic.code == code && !diagnostic.detail.empty());
    assert(read.skeleton.path == "retained" && read.worldTranslation == pxr::GfVec3d(3, 4, 5));
    assert(diagnostic.subject == selected.GetString() || !diagnostic.subject.empty());
}
void
Invalid()
{
    Reject({}, "MOTION_USD_STAGE");
    Reject(Stage(), "MOTION_USD_SKELETON_PATH", pxr::SdfPath());
    Reject(Stage(), "MOTION_USD_SKELETON_PATH", pxr::SdfPath("Rig/Skeleton"));
    Reject(Stage(), "MOTION_USD_SKELETON", pxr::SdfPath("/Missing"));
    auto stage = Stage();
    assert(pxr::UsdGeomSetStageUpAxis(stage, pxr::UsdGeomTokens->z));
    Reject(stage, "MOTION_USD_UP_AXIS");
    stage = Stage();
    assert(pxr::UsdGeomSetStageMetersPerUnit(stage, 0));
    Reject(stage, "MOTION_USD_UNITS");
    stage = Stage();
    pxr::UsdSkelSkeleton(stage->GetPrimAtPath(path)).GetRestTransformsAttr().Block();
    Reject(stage, "MOTION_USD_REST_COUNT");
    auto badTokens = [](pxr::VtTokenArray tokens, const char* code) {
        const auto s = Stage();
        const auto sk = pxr::UsdSkelSkeleton(s->GetPrimAtPath(path));
        assert(sk.GetJointsAttr().Set(tokens));
        Reject(s, code);
    };
    badTokens({pxr::TfToken("A"), pxr::TfToken("A"), pxr::TfToken("B")}, "MOTION_USD_JOINT_TOKEN");
    badTokens({pxr::TfToken("A/B"), pxr::TfToken("A"), pxr::TfToken("B")}, "MOTION_USD_TOPOLOGY");
    badTokens({pxr::TfToken("A"), pxr::TfToken("A/B/C"), pxr::TfToken("D")},
              "MOTION_USD_PARENT_MAPPING");
    badTokens({pxr::TfToken("/A"), pxr::TfToken("B"), pxr::TfToken("C")}, "MOTION_USD_JOINT_TOKEN");
    auto badMatrix = [](pxr::GfMatrix4d matrix, const char* code) {
        const auto s = Stage();
        const auto sk = pxr::UsdSkelSkeleton(s->GetPrimAtPath(path));
        pxr::VtMatrix4dArray rests;
        assert(sk.GetRestTransformsAttr().Get(&rests));
        rests[0] = matrix;
        assert(sk.GetRestTransformsAttr().Set(rests));
        Reject(s, code);
    };
    pxr::GfMatrix4d m(1);
    m[0][1] = 0.2;
    badMatrix(m, "MOTION_USD_SHEAR");
    m.SetIdentity();
    m[0][0] = -1;
    badMatrix(m, "MOTION_USD_REFLECTION");
    m.SetIdentity();
    m[0][0] = 0;
    badMatrix(m, "MOTION_USD_SCALE");
    m.SetIdentity();
    m[0][0] = 1e-50;
    badMatrix(m, "MOTION_USD_SCALE");
    m.SetIdentity();
    m[0][3] = 2e-12;
    badMatrix(m, "MOTION_USD_NONAFFINE");
    m.SetIdentity();
    m[3][3] = 1 + 2e-12;
    badMatrix(m, "MOTION_USD_NONAFFINE");
    m.SetIdentity();
    m[0][0] = std::numeric_limits<double>::infinity();
    badMatrix(m, "MOTION_USD_NONFINITE");
    m.SetIdentity();
    m[3][0] = 1e100;
    badMatrix(m, "MOTION_USD_FLOAT_RANGE");
    stage = Stage();
    assert(
        pxr::UsdGeomXform::Define(stage, pxr::SdfPath("/Rig")).AddScaleOp().Set(pxr::GfVec3f(2)));
    Reject(stage, "MOTION_USD_PLACEMENT_SCALE");
    SkeletonReadDiagnostic diagnostic;
    assert(!ReadSkeleton(Stage(), path, nullptr, &diagnostic) &&
           diagnostic.code == "MOTION_USD_OUTPUT");
    assert(!ReadSkeleton({}, path, nullptr, nullptr));
}
void
OwnedValues()
{
    auto stage = Stage();
    assert(pxr::UsdGeomSetStageMetersPerUnit(stage, 0.01));
    auto root = pxr::UsdGeomXform::Define(stage, pxr::SdfPath("/Rig"));
    pxr::GfMatrix4d placement(1);
    placement.SetRotate(pxr::GfRotation(pxr::GfVec3d(0, 1, 0), 90));
    placement.SetTranslateOnly(pxr::GfVec3d(300, 0, 500));
    assert(root.AddTransformOp().Set(placement));
    auto sk = pxr::UsdSkelSkeleton(stage->GetPrimAtPath(path));
    pxr::VtMatrix4dArray rests;
    assert(sk.GetRestTransformsAttr().Get(&rests));
    rests[0][3][3] = 1 + 2.220446049250313e-16;
    rests[1][0][3] = 5e-13;
    assert(sk.GetRestTransformsAttr().Set(rests));
    SkeletonStageRead read;
    SkeletonReadDiagnostic diagnostic{"old", "old", "old"};
    assert(ReadSkeleton(stage, path, &read, &diagnostic));
    assert(diagnostic.code.empty() && diagnostic.subject.empty() && diagnostic.detail.empty());
    assert(read.skeleton.restTransformsAuthored && read.skeleton.jointTokens.size() == 3);
    assert(read.parents == std::vector<int>({-1, 0, -1}));
    assert(read.metersPerUnit == 0.01 &&
           std::abs(read.skeleton.restTransforms[0][3][1] - 0.8) < 1e-12);
    assert(read.skeleton.restTransforms[0][3][3] == 1 &&
           read.skeleton.restTransforms[1][0][3] == 0);
    assert(read.skeleton.restTransforms[2][0][0] == 0.5 &&
           read.skeleton.restTransforms[2][2][2] == 2);
    assert(read.worldTranslation == pxr::GfVec3d(3, 0, 5));
    assert(std::abs(read.worldRotation.GetImaginary()[1] - std::sqrt(0.5)) < 1e-12);
    pxr::VtMatrix4dArray authored;
    assert(sk.GetRestTransformsAttr().Get(&authored) && authored == rests);
    const auto copy = read;
    assert(stage->RemovePrim(pxr::SdfPath("/Rig")));
    stage.Reset();
    assert(copy.skeleton.path == path.GetString() &&
           copy.skeleton.restTransforms == read.skeleton.restTransforms);
    assert(ReadSkeleton(Stage(), path, &read, nullptr));
}
void
CanonicalClip()
{
    auto stage = Stage();
    MotionStageRead permissive, read;
    SkeletonReadDiagnostic diagnostic;
    std::string error;
    assert(ReadMotionStage(stage, path.GetString(), &permissive, &error));
    assert(ReadCanonicalMotionStage(stage, path, &read, &diagnostic));
    assert(read.clip == permissive.clip && read.warnings == permissive.warnings);
    assert(read.animationPath == permissive.animationPath && read.timeCodesPerSecond == 60);
    assert(!read.metadata.contractVersion && read.skeleton.restTransformsAuthored);
    assert(read.clip.samples.size() == 2 && read.clip.samples[1].timestamp == 1);
    auto reject = [&](const pxr::UsdStagePtr& s, const char* code) {
        read.animationPath = "retained";
        assert(!ReadCanonicalMotionStage(s, path, &read, &diagnostic));
        assert(diagnostic.code == code && diagnostic.subject == path.GetString());
        assert(read.animationPath == "retained");
    };
    stage = Stage();
    assert(pxr::UsdGeomSetStageMetersPerUnit(stage, 0.01));
    reject(stage, "MOTION_USD_UNITS");
    stage = Stage();
    stage->SetTimeCodesPerSecond(0);
    reject(stage, "MOTION_USD_RATE");
    stage = Stage();
    assert(pxr::UsdGeomXform::Define(stage, pxr::SdfPath("/Rig"))
               .AddTranslateOp()
               .Set(pxr::GfVec3d(1, 0, 0)));
    reject(stage, "MOTION_USD_PLACEMENT");
    stage = Stage();
    assert(stage->RemovePrim(pxr::SdfPath("/Rig/Animation")));
    reject(stage, "MOTION_USD_READ");
    stage = Stage();
    pxr::UsdSkelSkeleton(stage->GetPrimAtPath(path)).GetRestTransformsAttr().Block();
    // The legacy reader still reports a usable clip with a rest fallback;
    // strict preparation refuses that profile without changing the legacy API.
    assert(ReadMotionStage(stage, path.GetString(), &permissive, &error));
    assert(!permissive.skeleton.restTransformsAuthored && !permissive.warnings.empty());
    reject(stage, "MOTION_USD_REST_COUNT");
}
void
CanonicalInputs()
{
    auto stage = Stage();
    auto prim = stage->DefinePrim(pxr::SdfPath("/Native"));
    assert(prim.CreateAttribute(pxr::TfToken("name"), pxr::SdfValueTypeNames->Token)
               .Set(pxr::TfToken("happy")));
    auto weight = prim.CreateAttribute(pxr::TfToken("weight"), pxr::SdfValueTypeNames->Float);
    assert(weight.Set(1.5f, 30));
    auto target = prim.CreateAttribute(pxr::TfToken("target"), pxr::SdfValueTypeNames->Point3f);
    assert(target.Set(pxr::GfVec3f(0), 15));
    MotionStageReadOptions options;
    options.channels.push_back({"/Native.name", "/Native.weight", "vrm:"});
    options.lookAtTargetAttributePath = "/Native.target";
    MotionStageRead expected, read;
    SkeletonReadDiagnostic diagnostic;
    std::string error;
    assert(ReadMotionStage(stage, path.GetString(), options, &expected, &error));
    assert(ReadCanonicalMotionStage(stage, path, options, &read, &diagnostic));
    assert(read.clip == expected.clip && read.clip.samples.size() == 4);
    assert(read.clip.samples[1].timestamp == 0.25);
    assert(read.clip.samples[1].lookAtTarget == pxr::GfVec3f(0));
    assert(*read.clip.samples[2].channels.Find("vrm:happy") == 1.5f);
    assert(!read.clip.samples[2].lookAtTarget);
    const auto retained = read.clip;
    assert(weight.Set(std::numeric_limits<float>::infinity(), 30));
    assert(!ReadCanonicalMotionStage(stage, path, options, &read, &diagnostic));
    assert(diagnostic.code == "MOTION_USD_READ" && read.clip == retained);
    assert(weight.Set(0.5f, 30));
    assert(pxr::UsdGeomSetStageMetersPerUnit(stage, 0.01));
    assert(!ReadCanonicalMotionStage(stage, path, options, &read, &diagnostic));
    assert(diagnostic.code == "MOTION_USD_UNITS" && read.clip == retained);
}
} // namespace
int
main()
{
    Invalid();
    OwnedValues();
    CanonicalClip();
    CanonicalInputs();
}
