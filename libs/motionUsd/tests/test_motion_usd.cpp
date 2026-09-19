// SPDX-License-Identifier: Apache-2.0
//
// The standalone motion stage, opened back through OpenUSD: what a consumer of
// the stage sees, not what the writer meant (USD_MAPPING.md §2-§5).
#include "motionUsd/ClipWriter.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec3h.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/dictionary.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/cache.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/skeletonQuery.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace
{

using openstrata::motion::HumanJoint;
using openstrata::motion::MotionClip;
using openstrata::motion::MotionPose;
using openstrata::motion::MotionStageOptions;
using openstrata::motion::MotionStageReport;

constexpr auto kHips = static_cast<std::size_t>(HumanJoint::Hips);
constexpr auto kSpine = static_cast<std::size_t>(HumanJoint::Spine);
constexpr auto kHead = static_cast<std::size_t>(HumanJoint::Head);

bool
NearlyEqual(double a, double b, double epsilon = 1e-5)
{
    return std::fabs(a - b) <= epsilon;
}

bool
SameOrientation(const pxr::GfQuatf& a, const pxr::GfQuatf& b)
{
    const float dot = std::fabs(pxr::GfDot(a.GetNormalized(), b.GetNormalized()));
    return dot > 1.0f - 1e-5f;
}

pxr::GfQuatf
RotationX(float degrees)
{
    const float radians = degrees * 3.14159265358979324f / 180.0f;
    return pxr::GfQuatf(std::cos(radians * 0.5f),
                        pxr::GfVec3f(std::sin(radians * 0.5f), 0.0f, 0.0f));
}

MotionPose
MakePose(double timestamp, float hipsDegrees, const pxr::GfVec3f& root)
{
    MotionPose pose;
    pose.timestamp = timestamp;
    pose.localRotations[kHips] = RotationX(hipsDegrees);
    pose.validRotations.set(kHips);
    pose.localRotations[kSpine] = RotationX(hipsDegrees * 0.5f);
    pose.validRotations.set(kSpine);
    pose.root.worldPosition = root;
    pose.root.hasPosition = true;
    return pose;
}

// Three samples at 30 Hz, stamped the way a producer computes them.
MotionClip
MakeClip()
{
    MotionClip clip;
    for (int frame = 0; frame < 3; ++frame)
    {
        clip.samples.push_back(MakePose(frame / 30.0, 30.0f * static_cast<float>(frame),
                                        pxr::GfVec3f(0.0f, 0.9f, 0.1f * static_cast<float>(frame))));
    }
    clip.startTime = 0.0;
    clip.endTime = 2.0 / 30.0;
    clip.nominalFrameRate = 30.0;
    clip.source.provider = "test-producer";
    return clip;
}

const pxr::VtValue&
Entry(const pxr::VtDictionary& dictionary, const char* key)
{
    const auto found = dictionary.find(key);
    assert(found != dictionary.end());
    return found->second;
}

std::string
TempPath(const char* name)
{
    return (std::filesystem::temp_directory_path() / name).generic_string();
}

pxr::UsdStageRefPtr
WriteAndOpen(const MotionClip& clip, const MotionStageOptions& options, const char* name,
             MotionStageReport* report = nullptr)
{
    const std::string path = TempPath(name);
    std::string error;
    const bool written = openstrata::motion::WriteMotionStage(path, clip, options, report, &error);
    if (!written)
    {
        std::fprintf(stderr, "WriteMotionStage failed: %s\n", error.c_str());
    }
    assert(written);
    // Reload from disk, so the test reads the saved file and not the layer the
    // writer still holds.
    pxr::SdfLayerRefPtr layer = pxr::SdfLayer::FindOrOpen(path);
    assert(layer);
    layer->Reload(/* force */ true);
    pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(layer);
    assert(stage);
    return stage;
}

void
TestTheStageHasTheMappingsShape()
{
    MotionStageOptions options;
    options.sourceFormat = "capture";
    options.rootMotionSource = "root.worldPosition";
    MotionStageReport report;
    const pxr::UsdStageRefPtr stage = WriteAndOpen(MakeClip(), options, "motionUsd_shape.usda", &report);

    assert(stage->GetDefaultPrim().GetPath() == pxr::SdfPath("/Animation"));
    assert(pxr::UsdGeomGetStageUpAxis(stage) == pxr::UsdGeomTokens->y);
    assert(pxr::UsdGeomGetStageMetersPerUnit(stage) == 1.0);
    assert(stage->GetTimeCodesPerSecond() == 30.0);
    assert(stage->GetStartTimeCode() == 0.0);
    assert(stage->GetEndTimeCode() == 2.0);

    const pxr::VtValue motion = stage->GetDefaultPrim().GetCustomDataByKey(pxr::TfToken("motion"));
    assert(motion.IsHolding<pxr::VtDictionary>());
    const pxr::VtDictionary& metadata = motion.UncheckedGet<pxr::VtDictionary>();
    assert(Entry(metadata, "contractVersion").Get<int>() == 1);
    assert(Entry(metadata, "jointVocabularyVersion").Get<int>() == 1);
    assert(Entry(metadata, "sourceFormat").Get<std::string>() == "capture");
    assert(Entry(metadata, "sourceProvider").Get<std::string>() == "test-producer");
    assert(Entry(metadata, "rootMotionSource").Get<std::string>() == "root.worldPosition");
    assert(Entry(metadata, "sampleCount").Get<int>() == 3);
    assert(NearlyEqual(Entry(metadata, "duration").Get<double>(), 2.0 / 30.0));
    assert(Entry(metadata, "nominalFrameRate").Get<double>() == 30.0);

    const pxr::UsdSkelSkeleton skeleton(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Skeleton")));
    assert(skeleton);
    pxr::VtTokenArray joints;
    skeleton.GetJointsAttr().Get(&joints);
    assert(joints == pxr::VtTokenArray({pxr::TfToken("hips"), pxr::TfToken("hips/spine")}));
    pxr::VtMatrix4dArray rests;
    skeleton.GetRestTransformsAttr().Get(&rests);
    // Identity except the hips translation, which is the first root position.
    assert(rests[0].ExtractTranslation() == pxr::GfVec3d(0.0, 0.9f, 0.0));
    assert(rests[1] == pxr::GfMatrix4d(1.0));

    const pxr::UsdSkelAnimation body(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Body")));
    assert(body);
    pxr::VtVec3hArray scales;
    assert(body.GetScalesAttr().Get(&scales));
    assert(scales == pxr::VtVec3hArray(2, pxr::GfVec3h(1.0f)));
    const pxr::UsdAttribute rate =
        body.GetPrim().GetAttribute(pxr::TfToken("motion:timeCodesPerSecond"));
    double rateValue = 0.0;
    assert(rate && rate.IsCustom() && rate.GetVariability() == pxr::SdfVariabilityUniform);
    assert(rate.Get(&rateValue) && rateValue == 30.0);

    // Whole frames, although 1/30 * 30 is not exactly 1 in a double.
    std::vector<double> times;
    body.GetRotationsAttr().GetTimeSamples(&times);
    assert(times == std::vector<double>({0.0, 1.0, 2.0}));

    std::vector<pxr::SdfPath> targets;
    pxr::UsdSkelBindingAPI(skeleton.GetPrim()).GetAnimationSourceRel().GetTargets(&targets);
    assert(targets == std::vector<pxr::SdfPath>({pxr::SdfPath("/Animation/Body")}));

    assert(report.jointCount == 2);
    assert(report.sampleCount == 3);
    assert(report.unauthoredChannels.empty());
    assert(report.unauthoredLookAtTargets == 0);
}

// What UsdSkel resolves, not what was authored: the check that catches a
// missing `scales`, which leaves every query succeeding at the rest pose.
void
TestUsdSkelResolvesTheMotion()
{
    const pxr::UsdStageRefPtr stage = WriteAndOpen(MakeClip(), {}, "motionUsd_resolve.usda");
    pxr::UsdSkelCache cache;
    const pxr::UsdSkelSkeleton skeleton(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Skeleton")));
    const pxr::UsdSkelSkeletonQuery query = cache.GetSkelQuery(skeleton);
    assert(query && query.GetAnimQuery());

    pxr::VtMatrix4dArray locals;
    assert(query.ComputeJointLocalTransforms(&locals, pxr::UsdTimeCode(2.0)));
    assert(locals.size() == 2);
    const pxr::GfQuatf hips(locals[0].ExtractRotationQuat());
    assert(SameOrientation(hips, RotationX(60.0f)));
    assert(NearlyEqual(locals[0].ExtractTranslation()[2], 0.2));
    const pxr::GfQuatf spine(locals[1].ExtractRotationQuat());
    assert(SameOrientation(spine, RotationX(30.0f)));
}

// A clip at a rate that is not 30 keeps its sample times; only the encoding is 30.
void
TestTimeCodesAreAlwaysThirtyPerSecond()
{
    MotionClip clip;
    for (int frame = 0; frame < 4; ++frame)
    {
        clip.samples.push_back(MakePose(frame / 60.0, 0.0f, pxr::GfVec3f(0.0f)));
    }
    clip.nominalFrameRate = 60.0;
    const pxr::UsdStageRefPtr stage = WriteAndOpen(clip, {}, "motionUsd_sixty.usda");
    assert(stage->GetTimeCodesPerSecond() == 30.0);
    std::vector<double> times;
    pxr::UsdSkelAnimation(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Body")))
        .GetRotationsAttr()
        .GetTimeSamples(&times);
    assert(times == std::vector<double>({0.0, 0.5, 1.0, 1.5}));
    // Undeclared span: the samples are the authority on it.
    assert(stage->GetEndTimeCode() == 1.5);
}

// Absent is not rest: a joint never observed is off the skeleton, a joint
// missing from one sample is authored at identity (holding is intake's), and a
// root missing from one sample is held rather than sent back to the rest.
void
TestAbsenceIsAuthoredAsAbsence()
{
    MotionClip clip;
    MotionPose rootOnly;
    rootOnly.timestamp = 0.0;
    rootOnly.root.worldPosition = pxr::GfVec3f(1.0f, 0.9f, 0.0f);
    rootOnly.root.hasPosition = true;
    MotionPose headOnly;
    headOnly.timestamp = 1.0 / 30.0;
    headOnly.localRotations[kHead] = RotationX(20.0f);
    headOnly.validRotations.set(kHead);
    MotionPose moved = rootOnly;
    moved.timestamp = 2.0 / 30.0;
    moved.root.worldPosition = pxr::GfVec3f(2.0f, 0.9f, 0.0f);
    clip.samples = {rootOnly, headOnly, moved};

    const pxr::UsdStageRefPtr stage = WriteAndOpen(clip, {}, "motionUsd_absence.usda");
    const pxr::UsdSkelAnimation body(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Body")));
    pxr::VtTokenArray joints;
    body.GetJointsAttr().Get(&joints);
    // The hips join on the root alone; the head parents to its nearest present
    // ancestor. Nothing between them is invented.
    assert(joints == pxr::VtTokenArray({pxr::TfToken("hips"), pxr::TfToken("hips/head")}));

    pxr::VtQuatfArray rotations;
    pxr::VtVec3fArray translations;
    body.GetRotationsAttr().Get(&rotations, pxr::UsdTimeCode(0.0));
    assert(SameOrientation(rotations[1], pxr::GfQuatf(1.0f)));
    body.GetRotationsAttr().Get(&rotations, pxr::UsdTimeCode(1.0));
    assert(SameOrientation(rotations[0], pxr::GfQuatf(1.0f)));
    assert(SameOrientation(rotations[1], RotationX(20.0f)));
    body.GetTranslationsAttr().Get(&translations, pxr::UsdTimeCode(1.0));
    assert(translations[0] == pxr::GfVec3f(1.0f, 0.9f, 0.0f));
    assert(translations[1] == pxr::GfVec3f(0.0f));
    body.GetTranslationsAttr().Get(&translations, pxr::UsdTimeCode(2.0));
    assert(translations[0] == pxr::GfVec3f(2.0f, 0.9f, 0.0f));
}

// What the mapping cannot hold yet is reported, not dropped in silence.
void
TestUnauthoredValuesAreReported()
{
    MotionClip clip = MakeClip();
    clip.samples[0].channels.Set("smile", 0.5f);
    clip.samples[1].channels.Set("blink", 1.0f);
    clip.samples[2].channels.Set("smile", 0.25f);
    clip.samples[2].lookAtTarget = pxr::GfVec3f(0.0f, 1.5f, 1.0f);
    MotionStageReport report;
    const pxr::UsdStageRefPtr stage = WriteAndOpen(clip, {}, "motionUsd_report.usda", &report);
    assert(report.unauthoredChannels == std::vector<std::string>({"blink", "smile"}));
    assert(report.unauthoredLookAtTargets == 1);
    assert(!stage->GetPrimAtPath(pxr::SdfPath("/Animation/Channels")));
}

void
TestRefusalsAuthorNothing()
{
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    std::string error;
    const auto refuses = [&](const MotionClip& clip)
    {
        error.clear();
        const bool authored = openstrata::motion::AuthorMotionStage(stage, clip, {}, nullptr, &error);
        return !authored && !error.empty() && !stage->GetPrimAtPath(pxr::SdfPath("/Animation"));
    };

    assert(refuses(MotionClip()));

    MotionClip nothingObserved;
    nothingObserved.samples.push_back(MotionPose());
    assert(refuses(nothingObserved));

    MotionClip notFinite = MakeClip();
    notFinite.samples[1].timestamp = std::numeric_limits<double>::quiet_NaN();
    assert(refuses(notFinite));

    MotionClip backwards = MakeClip();
    std::swap(backwards.samples[0], backwards.samples[2]);
    assert(refuses(backwards));

    // Two samples a fraction of a microsecond apart land on one time code.
    MotionClip collapsed = MakeClip();
    collapsed.samples[1].timestamp = collapsed.samples[0].timestamp + 1e-9;
    assert(refuses(collapsed));

    assert(!openstrata::motion::AuthorMotionStage(nullptr, MakeClip(), {}, nullptr, &error));

    // A stage that already holds a motion stage is not written over in place:
    // what the new clip did not overwrite would survive into it.
    const pxr::UsdStageRefPtr filled = pxr::UsdStage::CreateInMemory();
    assert(openstrata::motion::AuthorMotionStage(filled, MakeClip(), {}, nullptr, &error));
    assert(!openstrata::motion::AuthorMotionStage(filled, MakeClip(), {}, nullptr, &error));
}

// A refused write leaves the path as it was: no file where there was none, and
// an existing file untouched.
void
TestARefusedWriteTouchesNothing()
{
    std::string error;
    const std::string absent = TempPath("motionUsd_refused_absent.usda");
    std::filesystem::remove(absent);
    assert(!openstrata::motion::WriteMotionStage(absent, MotionClip(), {}, nullptr, &error));
    assert(!std::filesystem::exists(absent));

    const std::string existing = TempPath("motionUsd_refused_existing.usda");
    assert(openstrata::motion::WriteMotionStage(existing, MakeClip(), {}, nullptr, &error));
    const pxr::SdfLayerRefPtr held = pxr::SdfLayer::FindOrOpen(existing);
    assert(held);
    const auto size = std::filesystem::file_size(existing);
    MotionClip backwards = MakeClip();
    std::swap(backwards.samples[0], backwards.samples[2]);
    assert(!openstrata::motion::WriteMotionStage(existing, backwards, {}, nullptr, &error));
    assert(std::filesystem::file_size(existing) == size);
    // Nor emptied in memory for the caller still holding the layer.
    assert(held->GetPrimAtPath(pxr::SdfPath("/Animation/Body")));
}

// A producer that states its rest: the rig is the joint set, every joint holds
// its rest translation, an unturned joint keeps its rest rotation, and the
// provenance survives beside the motion.
void
TestAProducerRestIsTheSkeleton()
{
    openstrata::motion::MotionStageRest rest;
    rest.present.set(kHips);
    rest.present.set(kSpine);
    rest.present.set(kHead);
    rest.localTranslations[kHips] = pxr::GfVec3f(0.0f, 0.95f, 0.0f);
    rest.localTranslations[kSpine] = pxr::GfVec3f(0.0f, 0.1f, 0.0f);
    rest.localTranslations[kHead] = pxr::GfVec3f(0.0f, 0.5f, 0.0f);
    rest.localRotations[kHead] = RotationX(10.0f);

    // No sample turns the head, and the second sample carries no root.
    MotionClip clip = MakeClip();
    clip.samples[1].root.hasPosition = false;
    MotionStageOptions options;
    options.rest = rest;
    options.provenance = {{"profileId", "example-v1"}, {"producer", "Example"}};
    const pxr::UsdStageRefPtr stage = WriteAndOpen(clip, options, "motionUsd_rest.usda");

    const pxr::UsdSkelSkeleton skeleton(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Skeleton")));
    pxr::VtTokenArray joints;
    skeleton.GetJointsAttr().Get(&joints);
    assert(joints == pxr::VtTokenArray({pxr::TfToken("hips"), pxr::TfToken("hips/spine"),
                                        pxr::TfToken("hips/spine/head")}));
    pxr::VtMatrix4dArray rests;
    skeleton.GetRestTransformsAttr().Get(&rests);
    assert(rests[0].ExtractTranslation() == pxr::GfVec3d(0.0, 0.95f, 0.0));
    assert(rests[1].ExtractTranslation() == pxr::GfVec3d(0.0, 0.1f, 0.0));
    assert(SameOrientation(pxr::GfQuatf(rests[2].ExtractRotationQuat()), RotationX(10.0f)));

    const pxr::UsdSkelAnimation body(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Body")));
    pxr::VtVec3fArray translations;
    pxr::VtQuatfArray rotations;
    body.GetTranslationsAttr().Get(&translations, pxr::UsdTimeCode(1.0));
    body.GetRotationsAttr().Get(&rotations, pxr::UsdTimeCode(1.0));
    // The root held from the sample before, not sent back to the rest.
    assert(translations[0] == clip.samples[0].root.worldPosition);
    assert(translations[1] == pxr::GfVec3f(0.0f, 0.1f, 0.0f));
    assert(translations[2] == pxr::GfVec3f(0.0f, 0.5f, 0.0f));
    assert(SameOrientation(rotations[2], RotationX(10.0f)));

    const pxr::VtValue source = stage->GetDefaultPrim().GetCustomDataByKey(pxr::TfToken("source"));
    assert(source.IsHolding<pxr::VtDictionary>());
    const pxr::VtDictionary& provenance = source.UncheckedGet<pxr::VtDictionary>();
    assert(Entry(provenance, "profileId").Get<std::string>() == "example-v1");
    assert(Entry(provenance, "producer").Get<std::string>() == "Example");

    // A rest with no hips has nowhere to carry the root; a sample turning a
    // joint the rest does not carry is a clip of another rig.
    const pxr::UsdStageRefPtr empty = pxr::UsdStage::CreateInMemory();
    std::string error;
    MotionStageOptions noHips;
    noHips.rest = rest;
    noHips.rest->present.reset(kHips);
    assert(!openstrata::motion::AuthorMotionStage(empty, clip, noHips, nullptr, &error));
    MotionStageOptions narrow;
    narrow.rest = rest;
    narrow.rest->present.reset(kSpine);
    assert(!openstrata::motion::AuthorMotionStage(empty, clip, narrow, nullptr, &error));
    assert(!empty->GetPrimAtPath(pxr::SdfPath("/Animation")));
}

// Re-running a conversion over its previous output replaces it.
void
TestRewritingReplacesThePreviousStage()
{
    MotionClip longer = MakeClip();
    longer.samples.push_back(MakePose(3.0 / 30.0, 90.0f, pxr::GfVec3f(0.0f)));
    longer.samples.back().localRotations[kHead] = RotationX(10.0f);
    longer.samples.back().validRotations.set(kHead);
    WriteAndOpen(longer, {}, "motionUsd_rewrite.usda");
    const pxr::UsdStageRefPtr stage = WriteAndOpen(MakeClip(), {}, "motionUsd_rewrite.usda");
    std::vector<double> times;
    const pxr::UsdSkelAnimation body(stage->GetPrimAtPath(pxr::SdfPath("/Animation/Body")));
    body.GetRotationsAttr().GetTimeSamples(&times);
    assert(times.size() == 3);
    pxr::VtTokenArray joints;
    body.GetJointsAttr().Get(&joints);
    assert(joints.size() == 2);
}

} // namespace

int
main()
{
    TestTheStageHasTheMappingsShape();
    TestUsdSkelResolvesTheMotion();
    TestTimeCodesAreAlwaysThirtyPerSecond();
    TestAbsenceIsAuthoredAsAbsence();
    TestUnauthoredValuesAreReported();
    TestRefusalsAuthorNothing();
    TestARefusedWriteTouchesNothing();
    TestAProducerRestIsTheSkeleton();
    TestRewritingReplacesThePreviousStage();
    std::puts("motionUsd tests passed");
    return 0;
}
