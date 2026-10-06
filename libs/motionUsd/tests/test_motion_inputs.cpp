// SPDX-License-Identifier: Apache-2.0
#include "motionUsd/ClipReader.h"
#include "motionUsd/ClipWriter.h"

#include "pxr/base/gf/rotation.h"
#include "pxr/base/vt/array.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdGeom/xformCache.h"
#include "pxr/usd/usdSkel/animation.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>

using namespace openstrata::motion;

namespace {
std::string
Content(const pxr::UsdStagePtr& stage)
{
    std::string text;
    assert(stage->GetRootLayer()->ExportToString(&text));
    return text;
}

MotionClip
Clip()
{
    MotionClip clip;
    for (int frame : {0, 30, 60}) {
        MotionPose pose;
        pose.timestamp = frame / 30.0;
        pose.root.hasPosition = true;
        pose.root.worldPosition = pxr::GfVec3f(0, 1, frame / 30.0f);
        clip.samples.push_back(pose);
    }
    clip.startTime = 0;
    clip.endTime = 2;
    clip.nominalFrameRate = 30;
    return clip;
}

pxr::UsdStageRefPtr
Stage()
{
    auto stage = pxr::UsdStage::CreateInMemory();
    std::string error;
    assert(AuthorMotionStage(stage, Clip(), {}, nullptr, &error));
    return stage;
}

pxr::UsdAttribute
Gaze(const pxr::UsdStagePtr& stage)
{
    return stage->GetPrimAtPath(pxr::SdfPath("/Animation/Body"))
        .CreateAttribute(
            pxr::TfToken("motion:lookAtTarget"), pxr::SdfValueTypeNames->Point3f, true);
}

MotionStageRead
Read(const pxr::UsdStagePtr& stage, const MotionStageReadOptions& options = {})
{
    MotionStageRead read;
    std::string error;
    const bool ok = ReadMotionStage(stage, "/Animation/Skeleton", options, &read, &error);
    if (!ok)
        std::fprintf(stderr, "%s\n", error.c_str());
    assert(ok);
    return read;
}

void
Refused(const pxr::UsdStagePtr& stage, const MotionStageReadOptions& options = {})
{
    MotionStageRead read;
    std::string error;
    assert(!ReadMotionStage(stage, "", options, &read, &error));
    assert(!error.empty());
}

void
RoundTrip()
{
    auto clip = Clip();
    clip.samples[1].lookAtTarget = pxr::GfVec3f(0);
    clip.samples[2].lookAtTarget = pxr::GfVec3f(2, 3, 4);
    clip.samples[1].channels.Set("vrm:happy", 0);
    const auto path = std::filesystem::temp_directory_path() / "motionUsd_gaze_roundtrip.usda";
    MotionStageReport report;
    std::string error;
    assert(WriteMotionStage(path.string(), clip, {}, &report, &error));
    assert(report.unauthoredLookAtTargets == 0);
    auto layer = pxr::SdfLayer::FindOrOpen(path.string());
    assert(layer && layer->Reload(true));
    MotionStageRead read;
    assert(OpenMotionStage(path.string(), "", {}, &read, &error));
    assert(read.clip.samples.size() == 3);
    for (std::size_t i = 0; i < 3; ++i) {
        assert(read.clip.samples[i].lookAtTarget == clip.samples[i].lookAtTarget);
        assert(read.clip.samples[i].timestamp == clip.samples[i].timestamp);
        assert(read.clip.samples[i].channels == clip.samples[i].channels);
    }
    std::filesystem::remove(path);
}

void
DefaultsAndSparseKeys()
{
    auto stage = Stage();
    assert(!Read(stage).clip.samples.front().lookAtTarget);
    auto gaze = Gaze(stage);
    assert(!Read(stage).clip.samples.front().lookAtTarget); // Declared only.
    gaze.Set(pxr::GfVec3f(0));
    for (const auto& pose : Read(stage).clip.samples)
        assert(pose.lookAtTarget == pxr::GfVec3f(0));
    // A numeric timeline with keys uses the keys, not the authored default.
    gaze.Set(pxr::GfVec3f(1, 2, 3), 15);
    gaze.Set(pxr::GfVec3f(0), 45);
    const auto keyed = Read(stage).clip.samples;
    assert(keyed.size() == 5);
    assert(!keyed[0].lookAtTarget && !keyed[2].lookAtTarget && !keyed[4].lookAtTarget);
    assert(keyed[1].timestamp == 0.5 && keyed[1].lookAtTarget == pxr::GfVec3f(1, 2, 3));
    assert(keyed[3].timestamp == 1.5 && keyed[3].lookAtTarget == pxr::GfVec3f(0));
    gaze.Set(pxr::SdfValueBlock(), 45);
    assert(!Read(stage).clip.samples[3].lookAtTarget);
    gaze.Clear();
    gaze.Set(pxr::GfVec3f(0));
    const pxr::UsdSkelAnimation body(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Body")));
    body.GetRotationsAttr().Clear();
    body.GetTranslationsAttr().Clear();
    stage->SetStartTimeCode(12);
    auto constant = Read(stage).clip.samples;
    assert(constant.size() == 1 && constant[0].timestamp == 0.4);
    assert(constant[0].lookAtTarget == pxr::GfVec3f(0));
}

MotionStageReadOptions
NativeInputs(const pxr::UsdStagePtr& stage)
{
    auto expression = stage->DefinePrim(pxr::SdfPath("/Animation/Expressions/sanitized"));
    expression
        .CreateAttribute(pxr::TfToken("vrm:expressionName"),
                         pxr::SdfValueTypeNames->Token,
                         false,
                         pxr::SdfVariabilityUniform)
        .Set(pxr::TfToken("custom.face"));
    auto weight = expression.CreateAttribute(
        pxr::TfToken("vrm:expressionWeight"), pxr::SdfValueTypeNames->Float, false);
    weight.Set(0.0f, 15);
    weight.Set(1.75f, 45);
    auto lookAt = stage->DefinePrim(pxr::SdfPath("/Animation/LookAt"));
    lookAt
        .CreateAttribute(
            pxr::TfToken("vrm:lookAtOffsetFromHeadBone"), pxr::SdfValueTypeNames->Float3, false)
        .Set(pxr::GfVec3f(0, 0.06f, 0));
    lookAt.CreateAttribute(pxr::TfToken("vrm:lookAtTarget"), pxr::SdfValueTypeNames->Point3f, false)
        .Set(pxr::GfVec3f(0), 15);
    MotionStageReadOptions options;
    options.channels.push_back({"/Animation/Expressions/sanitized.vrm:expressionName",
                                "/Animation/Expressions/sanitized.vrm:expressionWeight",
                                "vrm:"});
    options.lookAtTargetAttributePath = "/Animation/LookAt.vrm:lookAtTarget";
    return options;
}

void
ExplicitProjectionAndPlacement()
{
    auto stage = Stage();
    auto options = NativeInputs(stage);
    const std::string before = Content(stage);
    const auto common = Read(stage).clip.samples;
    assert(common.size() == 3);
    for (const auto& pose : common)
        assert(pose.channels.entries.empty() && !pose.lookAtTarget);
    const auto projected = Read(stage, options).clip.samples;
    assert(projected.size() == 5);
    assert(projected[1].timestamp == 0.5 && projected[3].timestamp == 1.5);
    assert(*projected[1].channels.Find("vrm:custom.face") == 0.0f);
    assert(*projected[3].channels.Find("vrm:custom.face") == 1.75f);
    assert(projected[1].lookAtTarget == pxr::GfVec3f(0));
    assert(!projected[0].lookAtTarget && !projected[2].lookAtTarget && !projected[3].lookAtTarget);
    assert(projected[0].channels.entries.empty() && projected[2].channels.entries.empty());
    assert(!projected[1].channels.Find("vrm:sanitized"));
    assert(Content(stage) == before);
    stage->SetTimeCodesPerSecond(60);
    auto sourceTiming = Read(stage, options).clip.samples;
    assert(sourceTiming[1].timestamp == 0.25 && sourceTiming[3].timestamp == 0.75);
    assert(sourceTiming[1].lookAtTarget == pxr::GfVec3f(0));
    stage->SetTimeCodesPerSecond(30);
    auto weight = stage->GetAttributeAtPath(pxr::SdfPath(options.channels[0].valueAttributePath));
    weight.Clear();
    weight.Set(-0.25f);
    for (const auto& pose : Read(stage, options).clip.samples)
        assert(*pose.channels.Find("vrm:custom.face") == -0.25f);
    weight.Clear();
    for (const auto& pose : Read(stage, options).clip.samples)
        assert(pose.channels.entries.empty());
    // Clip points remain clip points under nonidentity skeleton placement.
    auto skeleton =
        pxr::UsdGeomXformable(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Skeleton")));
    skeleton.AddTranslateOp().Set(pxr::GfVec3d(10, 20, 30));
    skeleton.AddRotateYOp().Set(90.0f);
    auto target = stage->GetAttributeAtPath(pxr::SdfPath(options.lookAtTargetAttributePath));
    target.Clear();
    target.Set(pxr::GfVec3f(1, 2, 3));
    auto placed = Read(stage, options).clip.samples.front();
    assert(placed.lookAtTarget == pxr::GfVec3f(1, 2, 3));
    auto world = pxr::UsdGeomXformCache(pxr::UsdTimeCode(0))
                     .GetLocalToWorldTransform(skeleton.GetPrim())
                     .Transform(pxr::GfVec3d(*placed.lookAtTarget));
    assert((world - pxr::GfVec3d(13, 22, 29)).GetLength() < 1e-6);
    // An absent selected attribute has no value, never an origin fallback.
    options.lookAtTargetAttributePath = "/Animation/LookAt.vrm:unreported";
    assert(!Read(stage, options).clip.samples.front().lookAtTarget);
}

void
MalformedInputs()
{
    auto stage = Stage();
    auto gaze = Gaze(stage);
    gaze.Set(pxr::GfVec3f(std::numeric_limits<float>::infinity(), 0, 0));
    Refused(stage);
    gaze.Set(pxr::GfVec3f(0));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.01);
    Refused(stage);
    pxr::UsdGeomSetStageMetersPerUnit(stage, 1);
    pxr::UsdGeomSetStageUpAxis(stage, pxr::TfToken("Z"));
    Refused(stage);
    pxr::UsdGeomSetStageUpAxis(stage, pxr::TfToken("Y"));
    stage->GetPrimAtPath(pxr::SdfPath("/Animation/Body"))
        .RemoveProperty(pxr::TfToken("motion:lookAtTarget"));
    stage->GetPrimAtPath(pxr::SdfPath("/Animation/Body"))
        .CreateAttribute(pxr::TfToken("motion:lookAtTarget"), pxr::SdfValueTypeNames->Float, true)
        .Set(1.0f);
    Refused(stage);
    auto native = Stage();
    auto options = NativeInputs(native);
    auto duplicate = options;
    duplicate.channels.push_back(options.channels[0]);
    Refused(native, duplicate);
    auto collisionClip = Clip();
    collisionClip.samples[0].channels.Set("vrm:custom.face", 0);
    auto collision = pxr::UsdStage::CreateInMemory();
    std::string collisionError;
    assert(AuthorMotionStage(collision, collisionClip, {}, nullptr, &collisionError));
    Refused(collision, NativeInputs(collision));
    auto badPath = options;
    badPath.lookAtTargetAttributePath = "relative.attr";
    Refused(native, badPath);
    auto weight = native->GetAttributeAtPath(pxr::SdfPath(options.channels[0].valueAttributePath));
    weight.Set(pxr::SdfValueBlock(), 15);
    assert(!Read(native, options).clip.samples[1].channels.Find("vrm:custom.face"));
    weight.Set(std::numeric_limits<float>::quiet_NaN(), 15);
    Refused(native, options);
    weight.Set(0.0f, 15);
    auto name = native->GetAttributeAtPath(pxr::SdfPath(options.channels[0].nameAttributePath));
    name.Set(pxr::TfToken(""));
    Refused(native, options);
    name.Set(pxr::TfToken("custom.face"));
    name.Set(pxr::TfToken("changing"), 0);
    Refused(native, options);
    name.ClearAtTime(0);
    auto prim = native->GetPrimAtPath(pxr::SdfPath("/Animation/Expressions/sanitized"));
    prim.RemoveProperty(pxr::TfToken("vrm:expressionWeight"));
    prim.CreateAttribute(
            pxr::TfToken("vrm:expressionWeight"), pxr::SdfValueTypeNames->Double, false)
        .Set(1.0);
    Refused(native, options);
    auto clip = Clip();
    clip.samples[0].lookAtTarget = pxr::GfVec3f(0, std::numeric_limits<float>::quiet_NaN(), 0);
    auto empty = pxr::UsdStage::CreateInMemory();
    const std::string before = Content(empty);
    std::string error;
    assert(!AuthorMotionStage(empty, clip, {}, nullptr, &error));
    assert(error.find("look-at") != std::string::npos);
    assert(Content(empty) == before);
}
} // namespace

int
main()
{
    RoundTrip();
    DefaultsAndSparseKeys();
    ExplicitProjectionAndPlacement();
    MalformedInputs();
    std::puts("motionUsd input tests passed");
}
