// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "motionUsd/ClipReader.h"
#include "motionUsd/ClipWriter.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"

inline bool
CheckMotionInputs()
{
    using namespace openstrata::motion;
    MotionClip clip;
    for (int i = 0; i < 3; ++i) {
        MotionPose pose;
        pose.timestamp = i;
        pose.root.hasPosition = true;
        pose.root.worldPosition = pxr::GfVec3f(0, 1, 0);
        clip.samples.push_back(pose);
    }
    clip.samples[1].lookAtTarget = pxr::GfVec3f(0);
    clip.samples[2].lookAtTarget = pxr::GfVec3f(1, 2, 3);
    auto stage = pxr::UsdStage::CreateInMemory();
    MotionStageReport report;
    std::string error;
    if (!AuthorMotionStage(stage, clip, {}, &report, &error) || report.unauthoredLookAtTargets != 0)
        return false;
    MotionStageRead read;
    if (!ReadMotionStage(stage, "", &read, &error) || read.clip.samples.size() != 3)
        return false;
    for (std::size_t i = 0; i < 3; ++i)
        if (read.clip.samples[i].lookAtTarget != clip.samples[i].lookAtTarget)
            return false;
    const auto expression = stage->DefinePrim(pxr::SdfPath("/Animation/Expressions/safe_path"));
    expression
        .CreateAttribute(pxr::TfToken("vrm:expressionName"),
                         pxr::SdfValueTypeNames->Token,
                         false,
                         pxr::SdfVariabilityUniform)
        .Set(pxr::TfToken("custom.face"));
    const auto weight = expression.CreateAttribute(
        pxr::TfToken("vrm:expressionWeight"), pxr::SdfValueTypeNames->Float, false);
    weight.Set(0.0f, 15);
    weight.Set(1.5f, 45);
    const auto gaze = stage->DefinePrim(pxr::SdfPath("/Animation/LookAt"))
                          .CreateAttribute(pxr::TfToken("vrm:lookAtTarget"),
                                           pxr::SdfValueTypeNames->Point3f,
                                           false);
    gaze.Set(pxr::GfVec3f(0));
    MotionStageReadOptions options;
    options.channels.push_back({"/Animation/Expressions/safe_path.vrm:expressionName",
                                "/Animation/Expressions/safe_path.vrm:expressionWeight",
                                "vrm:"});
    options.lookAtTargetAttributePath = "/Animation/LookAt.vrm:lookAtTarget";
    if (!ReadMotionStage(stage, "", options, &read, &error) || read.clip.samples.size() != 5)
        return false;
    const auto& samples = read.clip.samples;
    const float* zero = samples[1].channels.Find("vrm:custom.face");
    const float* outside = samples[3].channels.Find("vrm:custom.face");
    if (!zero || *zero != 0 || !outside || *outside != 1.5f || samples[1].timestamp != 0.5 ||
        samples[3].timestamp != 1.5)
        return false;
    for (std::size_t i = 0; i < samples.size(); ++i)
        if (samples[i].lookAtTarget != pxr::GfVec3f(0) ||
            (i % 2 == 0 && !samples[i].channels.entries.empty()))
            return false;
    return true;
}
