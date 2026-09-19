// SPDX-License-Identifier: Apache-2.0
#include "motionUsd/ClipWriter.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec3h.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/dictionary.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/skeleton.h"

#include <bitset>
#include <cmath>
#include <set>

namespace openstrata::motion
{
namespace
{

constexpr double kFrameSnap = 1e-6;

// Where a time in seconds is written. Sample times are authoritative and a time
// code is only their encoding (USD_MAPPING.md §4.1), so the snap never moves a
// time by more than a microsecond of a frame; it removes the representation
// error of `k / rate * 30`, which a reader comparing time codes would otherwise
// see as a frame that is not quite a frame.
double
TimeCodeFor(double seconds)
{
    const double timeCode = seconds * MotionStageTimeCodesPerSecond;
    const double frame = std::round(timeCode);
    return std::fabs(timeCode - frame) <= kFrameSnap ? frame : timeCode;
}

} // namespace

bool
AuthorMotionStage(const pxr::UsdStagePtr& stage, const MotionClip& clip,
                  const MotionStageOptions& options, MotionStageReport* report,
                  std::string* error)
{
    if (!stage)
    {
        *error = "no stage to author into";
        return false;
    }
    if (clip.samples.empty())
    {
        *error = "the clip has no sample";
        return false;
    }
    // A stage that already holds a motion stage would keep whatever the new
    // clip does not overwrite -- a joint's old time samples, a prim this
    // writer no longer authors -- and read as one clip made of two.
    if (stage->GetPrimAtPath(pxr::SdfPath("/Animation")))
    {
        *error = "the stage already holds /Animation";
        return false;
    }

    // Every check before any authoring, so a refusal leaves the stage as it was.
    std::vector<double> timeCodes;
    timeCodes.reserve(clip.samples.size());
    for (const MotionPose& pose : clip.samples)
    {
        if (!std::isfinite(pose.timestamp))
        {
            *error = "a sample's timestamp is not finite";
            return false;
        }
        const double timeCode = TimeCodeFor(pose.timestamp);
        if (!timeCodes.empty() && timeCode <= timeCodes.back())
        {
            // Two samples on one time code would leave only the second, without
            // a word; a clip out of order would be authored as another clip.
            *error = "sample timestamps do not increase";
            return false;
        }
        timeCodes.push_back(timeCode);
    }

    // A joint exists in the clip when any sample observed it. A joint the
    // producer never solved is simply absent -- it is not authored at rest,
    // because a joint that is present and unmoving means something different
    // downstream from a joint that was never observed.
    std::bitset<HumanJointCount> present;
    bool observedRoot = false;
    std::set<std::string> channels;
    std::size_t lookAtTargets = 0;
    for (const MotionPose& pose : clip.samples)
    {
        present |= pose.validRotations;
        observedRoot = observedRoot || pose.root.hasPosition;
        for (const MotionChannel& channel : pose.channels.entries)
        {
            channels.insert(channel.name);
        }
        lookAtTargets += pose.lookAtTarget ? 1 : 0;
    }

    // The one joint that is not purely a rotation question. Root translation is
    // authored onto the hips and nowhere else, so a producer that reports a
    // root position while never solving a hips *rotation* would otherwise drop
    // its whole root motion here without a word. The hips join the joint set
    // on the strength of the root observation; their rotation track falls back
    // to identity below, exactly as any unobserved joint's does.
    const auto hips = static_cast<std::size_t>(HumanJoint::Hips);
    if (observedRoot)
    {
        present.set(hips);
    }
    if (!present.any())
    {
        *error = "the clip observes no joint and no root";
        return false;
    }

    std::vector<HumanJoint> joints;
    pxr::VtTokenArray jointTokens;
    for (std::size_t index = 0; index < HumanJointCount; ++index)
    {
        if (!present.test(index))
        {
            continue;
        }
        const auto joint = static_cast<HumanJoint>(index);
        joints.push_back(joint);
        jointTokens.push_back(pxr::TfToken(HumanJointPath(joint, present)));
    }

    // The span the clip declares, or the samples' own when it declares none --
    // the rule `Resample` applies to a clip whose interval was left at its
    // default.
    double start = clip.startTime;
    double end = clip.endTime;
    if (!(end > start))
    {
        start = clip.samples.front().timestamp;
        end = clip.samples.back().timestamp;
    }

    pxr::UsdGeomSetStageUpAxis(stage, pxr::UsdGeomTokens->y);
    pxr::UsdGeomSetStageMetersPerUnit(stage, 1.0);
    stage->SetTimeCodesPerSecond(MotionStageTimeCodesPerSecond);
    stage->SetFramesPerSecond(MotionStageTimeCodesPerSecond);
    stage->SetStartTimeCode(TimeCodeFor(start));
    stage->SetEndTimeCode(TimeCodeFor(end));

    const pxr::SdfPath rootPath("/Animation");
    const pxr::UsdPrim root = pxr::UsdGeomScope::Define(stage, rootPath).GetPrim();
    stage->SetDefaultPrim(root);

    // USD_MAPPING.md §5. One dictionary rather than colon-separated keys, which
    // USD would expand into the same thing.
    pxr::VtDictionary metadata;
    metadata["contractVersion"] = pxr::VtValue(MotionStageContractVersion);
    metadata["jointVocabularyVersion"] = pxr::VtValue(HumanJointVocabularyVersion);
    if (!options.sourceFormat.empty())
    {
        metadata["sourceFormat"] = pxr::VtValue(options.sourceFormat);
    }
    if (!clip.source.provider.empty())
    {
        metadata["sourceProvider"] = pxr::VtValue(clip.source.provider);
    }
    if (!options.rootMotionSource.empty())
    {
        metadata["rootMotionSource"] = pxr::VtValue(options.rootMotionSource);
    }
    metadata["duration"] = pxr::VtValue(end - start);
    metadata["sampleCount"] = pxr::VtValue(static_cast<int>(clip.samples.size()));
    metadata["nominalFrameRate"] = pxr::VtValue(clip.nominalFrameRate);
    root.SetCustomDataByKey(pxr::TfToken("motion"), pxr::VtValue(metadata));

    // A clip carries no rest pose -- its rotations are relative to the
    // canonical rest, not the rest itself. Authoring identity rests makes a
    // retargeter's rest-pose correction a no-op, which is the honest reading.
    // The one exception is the hips translation: it is seeded with the first
    // observed root position, so root motion arrives downstream as a delta from
    // where the motion started rather than as an absolute height
    // (USD_MAPPING.md §3).
    pxr::GfVec3f hipsRest(0.0f);
    for (const MotionPose& pose : clip.samples)
    {
        if (pose.root.hasPosition)
        {
            hipsRest = pose.root.worldPosition;
            break;
        }
    }

    const pxr::SdfPath skeletonPath = rootPath.AppendChild(pxr::TfToken("Skeleton"));
    const pxr::UsdSkelSkeleton skeleton = pxr::UsdSkelSkeleton::Define(stage, skeletonPath);
    pxr::VtMatrix4dArray restTransforms;
    restTransforms.reserve(joints.size());
    for (const HumanJoint joint : joints)
    {
        pxr::GfMatrix4d rest(1.0);
        if (joint == HumanJoint::Hips)
        {
            rest.SetTranslate(pxr::GfVec3d(hipsRest[0], hipsRest[1], hipsRest[2]));
        }
        restTransforms.push_back(rest);
    }
    skeleton.CreateJointsAttr(pxr::VtValue(jointTokens));
    skeleton.CreateRestTransformsAttr(pxr::VtValue(restTransforms));

    const pxr::SdfPath bodyPath = rootPath.AppendChild(pxr::TfToken("Body"));
    const pxr::UsdSkelAnimation body = pxr::UsdSkelAnimation::Define(stage, bodyPath);
    body.CreateJointsAttr(pxr::VtValue(jointTokens));
    pxr::UsdAttribute translations = body.CreateTranslationsAttr();
    pxr::UsdAttribute rotations = body.CreateRotationsAttr();
    // UsdSkel fetches translations, rotations and scales as a unit, and
    // `scales` has no schema fallback: omitting it does not mean "this clip
    // animates no scale", it silently drops the whole animation and leaves the
    // skeleton at its rest pose. Scale stays un-animated -- this constant
    // identity array exists only so the clip evaluates.
    const pxr::VtVec3hArray identityScales(joints.size(), pxr::GfVec3h(1.0f));
    body.CreateScalesAttr(pxr::VtValue(identityScales));
    // The rate again, from the same number as the stage metadata, because an
    // OpenExec computation cannot read stage metadata (EXEC_CONTRACT.md §5.1).
    body.GetPrim()
        .CreateAttribute(pxr::TfToken("motion:timeCodesPerSecond"), pxr::SdfValueTypeNames->Double,
                         /* custom */ true, pxr::SdfVariabilityUniform)
        .Set(MotionStageTimeCodesPerSecond);

    // The last placement this clip authored, so a sample that reports none
    // does not send the body somewhere. It starts at the rest, which is where a
    // clip whose *first* samples report no root has to begin.
    //
    // A missing root is not a missing joint and the fallbacks are not
    // symmetric, which is why this is held where the rotation below is not. An
    // unobserved joint has a neutral value -- the rest rotation says "this
    // joint is not turned" -- so authoring it states an absence. A root
    // position has no neutral value: the rest is *a place*, and authoring it
    // for one sample between two that reported their own teleports the body to
    // wherever the motion started and back. Holding states the same absence
    // without inventing the trip.
    pxr::GfVec3f hipsHeld = hipsRest;
    for (std::size_t sample = 0; sample < clip.samples.size(); ++sample)
    {
        const MotionPose& pose = clip.samples[sample];
        pxr::VtVec3fArray valuesT;
        pxr::VtQuatfArray valuesR;
        valuesT.reserve(joints.size());
        valuesR.reserve(joints.size());
        if (pose.root.hasPosition)
        {
            hipsHeld = pose.root.worldPosition;
        }
        for (const HumanJoint joint : joints)
        {
            const auto slot = static_cast<std::size_t>(joint);
            valuesT.push_back(joint == HumanJoint::Hips ? hipsHeld : pxr::GfVec3f(0.0f));
            // A sample that did not observe a joint authors the rest rotation
            // rather than the previous sample's: holding is an intake policy
            // (MissingJointPolicy), and re-deciding it here would hide which
            // policy actually ran.
            valuesR.push_back(pose.validRotations.test(slot)
                                  ? pose.localRotations[slot]
                                  : pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f)));
        }
        translations.Set(valuesT, timeCodes[sample]);
        rotations.Set(valuesR, timeCodes[sample]);
    }

    pxr::UsdSkelBindingAPI::Apply(skeleton.GetPrim()).CreateAnimationSourceRel().SetTargets({bodyPath});

    if (report)
    {
        report->jointCount = joints.size();
        report->sampleCount = clip.samples.size();
        report->unauthoredChannels.assign(channels.begin(), channels.end());
        report->unauthoredLookAtTargets = lookAtTargets;
    }
    return true;
}

bool
WriteMotionStage(const std::string& path, const MotionClip& clip,
                 const MotionStageOptions& options, MotionStageReport* report,
                 std::string* error)
{
    // Authored in memory first, so a refused clip leaves the path as it was:
    // `SdfLayer::CreateNew` writes an empty file at once, and clearing an
    // existing layer before the clip is checked would empty it in memory for
    // anyone else holding it.
    const pxr::UsdStageRefPtr scratch = pxr::UsdStage::CreateInMemory();
    if (!AuthorMotionStage(scratch, clip, options, report, error))
    {
        return false;
    }

    // Re-running a conversion over a previous output is the normal case, so an
    // existing layer is replaced instead of failing the way UsdStage::CreateNew
    // would.
    pxr::SdfLayerRefPtr layer = pxr::SdfLayer::FindOrOpen(path);
    if (!layer)
    {
        layer = pxr::SdfLayer::CreateNew(path);
    }
    if (!layer)
    {
        *error = "could not create output layer: " + path;
        return false;
    }
    layer->TransferContent(scratch->GetRootLayer());
    if (!layer->Save())
    {
        *error = "could not save output layer: " + path;
        return false;
    }
    return true;
}

} // namespace openstrata::motion
