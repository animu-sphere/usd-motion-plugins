// SPDX-License-Identifier: Apache-2.0
#include "motionUsd/SkeletonReader.h"
#include "pxr/base/vt/dictionary.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include <cassert>
#include <cmath>
#include <limits>

namespace {
using namespace openstrata::motion;
const pxr::SdfPath skeletonPath("/Skeleton");
constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double nan = std::numeric_limits<double>::quiet_NaN();

pxr::UsdStageRefPtr
Stage(double rate, const std::vector<double>& keys = {})
{
    auto stage = pxr::UsdStage::CreateInMemory();
    assert(pxr::UsdGeomSetStageUpAxis(stage, pxr::UsdGeomTokens->y));
    assert(pxr::UsdGeomSetStageMetersPerUnit(stage, 1));
    stage->SetTimeCodesPerSecond(rate);
    const auto skeleton = pxr::UsdSkelSkeleton::Define(stage, skeletonPath);
    assert(skeleton.CreateJointsAttr().Set(pxr::VtTokenArray{pxr::TfToken("hips")}));
    assert(skeleton.CreateRestTransformsAttr().Set(pxr::VtMatrix4dArray{pxr::GfMatrix4d(1)}));
    stage->SetDefaultPrim(skeleton.GetPrim());
    const auto animation = pxr::UsdSkelAnimation::Define(stage, pxr::SdfPath("/Body"));
    assert(animation.CreateJointsAttr().Set(pxr::VtTokenArray{pxr::TfToken("hips")}));
    assert(pxr::UsdSkelBindingAPI::Apply(skeleton.GetPrim())
               .CreateAnimationSourceRel().SetTargets({animation.GetPath()}));
    auto rotations = animation.CreateRotationsAttr();
    if (keys.empty()) {
        assert(rotations.Set(pxr::VtQuatfArray{pxr::GfQuatf(1)}));
    } else {
        for (double key : keys)
            assert(rotations.Set(pxr::VtQuatfArray{pxr::GfQuatf(1)}, key));
    }
    return stage;
}

void
SharedSampleRule()
{
    MotionStageSample sample;
    sample.hasTimeCode = true;
    sample.timeCode = -120;
    sample.timeCodesPerSecond = 60;
    assert(PoseFromStageSample(sample)->timestamp == -2);
    for (double rate : {0.0, -1.0, infinity, -infinity, nan}) {
        sample.timeCodesPerSecond = rate;
        assert(!PoseFromStageSample(sample));
        sample.hasTimeCode = false;
        assert(!PoseFromStageSample(sample));
        sample.hasTimeCode = true;
    }
    sample.timeCodesPerSecond = 60;
    for (double time : {infinity, -infinity, nan}) {
        sample.timeCode = time;
        assert(!PoseFromStageSample(sample));
        // At default time, the unused numeric time code does not stamp a pose.
        sample.hasTimeCode = false;
        assert(PoseFromStageSample(sample)->timestamp == 0);
        sample.hasTimeCode = true;
    }
    sample.timeCode = std::numeric_limits<double>::max();
    sample.timeCodesPerSecond = 0.5;
    assert(!PoseFromStageSample(sample));
}

void
Reject(const pxr::UsdStagePtr& stage, const char* code)
{
    MotionStageRead read;
    read.animationPath = "retained";
    read.clip.samples.emplace_back();
    read.clip.samples[0].timestamp = 7;
    const auto retained = read.clip;
    SkeletonReadDiagnostic diagnostic;
    assert(!ReadCanonicalMotionStage(stage, skeletonPath, &read, &diagnostic));
    assert(diagnostic.code == code && diagnostic.subject == skeletonPath.GetString());
    assert(!diagnostic.detail.empty());
    if (diagnostic.code == "MOTION_USD_READ")
        assert(diagnostic.detail.find("time code") != std::string::npos);
    assert(read.animationPath == "retained" && read.clip == retained);
}

void
RatesAndDefaults()
{
    for (double rate : {0.0, -1.0, infinity, -infinity, nan}) {
        const auto stage = Stage(rate, {0, 60});
        Reject(stage, "MOTION_USD_RATE");
        MotionStageRead read;
        std::string error;
        assert(ReadMotionStage(stage, skeletonPath.GetString(), &read, &error));
        assert(read.timeCodesPerSecond == 30 && read.clip.samples[1].timestamp == 2);
        assert(!read.warnings.empty());
    }
    auto stage = Stage(24);
    stage->SetStartTimeCode(-48);
    // Playback bounds do not crop authored keys or establish clip duration.
    stage->SetEndTimeCode(-24);
    MotionStageRead read;
    SkeletonReadDiagnostic diagnostic;
    assert(ReadCanonicalMotionStage(stage, skeletonPath, &read, &diagnostic));
    assert(read.clip.samples.size() == 1 && read.clip.samples[0].timestamp == -2);
    assert(read.clip.startTime == -2 && read.clip.endTime == -2);
    for (double start : {infinity, -infinity, nan}) {
        stage->SetStartTimeCode(start);
        Reject(stage, "MOTION_USD_READ");
    }
    stage = Stage(24, {-48, 0, 48});
    stage->SetStartTimeCode(100);
    stage->SetEndTimeCode(200);
    assert(ReadCanonicalMotionStage(stage, skeletonPath, &read, &diagnostic));
    assert(read.clip.samples.size() == 3 && read.clip.startTime == -2 && read.clip.endTime == 2);
    for (double nominal : {0.0, -1.0, infinity, -infinity, nan, 120.0}) {
        pxr::VtDictionary metadata;
        metadata["nominalFrameRate"] = pxr::VtValue(nominal);
        stage->GetDefaultPrim().SetCustomDataByKey(pxr::TfToken("motion"), pxr::VtValue(metadata));
        assert(ReadCanonicalMotionStage(stage, skeletonPath, &read, &diagnostic));
        assert(read.clip.nominalFrameRate == (nominal == 120 ? 120 : 24));
        assert(read.metadata.nominalFrameRate);
        assert(std::isnan(nominal) ? std::isnan(*read.metadata.nominalFrameRate)
                                   : *read.metadata.nominalFrameRate == nominal);
        assert(nominal == 120 ? read.warnings.empty() : !read.warnings.empty());
    }
}

void
UnrepresentableTimes()
{
    const double largest = std::numeric_limits<double>::max();
    Reject(Stage(0.5, {largest}), "MOTION_USD_READ");
    Reject(Stage(1, {-largest, largest}), "MOTION_USD_READ");
    // Two distinct source instants collapse when converted to double seconds.
    const double tiny = std::numeric_limits<double>::denorm_min();
    Reject(Stage(2, {0, tiny}), "MOTION_USD_READ");

    // The same policy applies to supplementary input keys, including when
    // the body is default-valued and cannot supply a usable timeline itself.
    for (bool gaze : {false, true}) {
        auto stage = Stage(0.5);
        auto prim = stage->GetPrimAtPath(pxr::SdfPath("/Body"));
        if (gaze) {
            assert(prim.CreateAttribute(pxr::TfToken("motion:lookAtTarget"),
                                        pxr::SdfValueTypeNames->Point3f)
                       .Set(pxr::GfVec3f(0), largest));
        } else {
            auto channel = stage->DefinePrim(pxr::SdfPath("/Channel"));
            assert(channel.CreateAttribute(pxr::TfToken("motion:channelName"),
                                            pxr::SdfValueTypeNames->String).Set(std::string("face")));
            assert(channel.CreateAttribute(pxr::TfToken("motion:channelValue"),
                                            pxr::SdfValueTypeNames->Float).Set(0.5f, largest));
        }
        Reject(stage, "MOTION_USD_READ");
    }
    auto stage = Stage(2, {0});
    auto native = stage->DefinePrim(pxr::SdfPath("/Selected"));
    assert(native.CreateAttribute(pxr::TfToken("name"), pxr::SdfValueTypeNames->Token)
               .Set(pxr::TfToken("face")));
    assert(native.CreateAttribute(pxr::TfToken("weight"), pxr::SdfValueTypeNames->Float)
               .Set(0.5f, tiny));
    MotionStageReadOptions options;
    options.channels.push_back({"/Selected.name", "/Selected.weight", "custom:"});
    MotionStageRead read;
    read.animationPath = "retained";
    SkeletonReadDiagnostic diagnostic;
    assert(!ReadCanonicalMotionStage(stage, skeletonPath, options, &read, &diagnostic));
    assert(diagnostic.code == "MOTION_USD_READ" &&
           diagnostic.detail.find("increasing seconds") != std::string::npos);
    assert(read.animationPath == "retained");
}
} // namespace

int
main()
{
    SharedSampleRule();
    RatesAndDefaults();
    UnrepresentableTimes();
}
