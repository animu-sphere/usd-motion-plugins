// SPDX-License-Identifier: Apache-2.0
#include "motionUsd/ClipReader.h"

#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/vec3h.h"
#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/dictionary.h"
#include "pxr/base/vt/types.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usd/timeCode.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/skeleton.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string_view>

namespace openstrata::motion
{
namespace
{

const pxr::TfToken kChannelName("motion:channelName");
const pxr::TfToken kChannelValue("motion:channelValue");
const pxr::TfToken kRate("motion:timeCodesPerSecond");
const pxr::TfToken kMotion("motion");
const pxr::TfToken kSource("source");

// The semantic joint tokens the motion contract defines are paths whose leaf
// is the joint name (`hips`, `hips/spine`, `hips/spine/chest`). Reading the
// leaf back is the documented inverse, not a name heuristic.
std::optional<HumanJoint>
JointForToken(const std::string& jointToken)
{
    const std::size_t separator = jointToken.rfind('/');
    const std::string_view leaf = separator == std::string::npos
                                      ? std::string_view(jointToken)
                                      : std::string_view(jointToken).substr(separator + 1);
    if (leaf.empty())
    {
        return std::nullopt;
    }
    return FindHumanJoint(leaf);
}

// Finds the skeleton to read against: the named one, otherwise the first
// `UsdSkelSkeleton` in stage order. Reporting which one is what makes a
// caller's override actionable when a stage carries several.
bool
FindSkeleton(const pxr::UsdStagePtr& stage, const std::string& named,
             pxr::UsdSkelSkeleton* skeleton, std::string* error)
{
    if (!named.empty())
    {
        if (!pxr::SdfPath::IsValidPathString(named))
        {
            *error = "not a valid prim path: " + named;
            return false;
        }
        const pxr::UsdPrim prim = stage->GetPrimAtPath(pxr::SdfPath(named));
        if (!prim)
        {
            *error = "no prim at " + named;
            return false;
        }
        *skeleton = pxr::UsdSkelSkeleton(prim);
        if (!*skeleton)
        {
            *error = named + " is not a UsdSkelSkeleton";
            return false;
        }
        return true;
    }

    for (const pxr::UsdPrim& prim : stage->Traverse())
    {
        if (prim.IsA<pxr::UsdSkelSkeleton>())
        {
            *skeleton = pxr::UsdSkelSkeleton(prim);
            return true;
        }
    }
    *error = "the stage has no UsdSkelSkeleton";
    return false;
}

// The skeleton's two arrays, as values. A skeleton with no usable
// `restTransforms` is identity and says so, rather than falling back to
// `bindTransforms`: those are world-space and cannot become local rests
// without the topology walk, so substituting them would answer a different
// question in the same field.
bool
ReadSkeletonValues(const pxr::UsdSkelSkeleton& skeleton, MotionStageSkeleton* out,
                   std::vector<std::string>* warnings)
{
    out->path = skeleton.GetPath().GetString();

    pxr::VtTokenArray joints;
    if (!skeleton.GetJointsAttr().Get(&joints) || joints.empty())
    {
        return false;
    }
    out->jointTokens.reserve(joints.size());
    for (const pxr::TfToken& joint : joints)
    {
        out->jointTokens.push_back(joint.GetString());
    }

    pxr::VtMatrix4dArray restTransforms;
    if (skeleton.GetRestTransformsAttr().Get(&restTransforms) &&
        restTransforms.size() == joints.size())
    {
        out->restTransforms.assign(restTransforms.begin(), restTransforms.end());
        out->restTransformsAuthored = true;
    }
    else
    {
        warnings->push_back("skeleton <" + out->path +
                            "> has no usable restTransforms; assuming an identity rest pose");
        out->restTransforms.assign(joints.size(), pxr::GfMatrix4d(1.0));
        out->restTransformsAuthored = false;
    }
    return true;
}

// `/Animation.customData.motion` and `customData.source`, read back (§5).
void
ReadMetadata(const pxr::UsdPrim& prim, MotionStageMetadata* metadata)
{
    if (!prim)
    {
        return;
    }

    const pxr::VtValue motion = prim.GetCustomDataByKey(kMotion);
    if (motion.IsHolding<pxr::VtDictionary>())
    {
        const pxr::VtDictionary& dictionary = motion.UncheckedGet<pxr::VtDictionary>();
        const auto readInt = [&dictionary](const char* key) -> std::optional<int>
        {
            const auto entry = dictionary.find(key);
            if (entry == dictionary.end() || !entry->second.IsHolding<int>())
            {
                return std::nullopt;
            }
            return entry->second.UncheckedGet<int>();
        };
        const auto readString = [&dictionary](const char* key) -> std::string
        {
            const auto entry = dictionary.find(key);
            if (entry == dictionary.end() || !entry->second.IsHolding<std::string>())
            {
                return std::string();
            }
            return entry->second.UncheckedGet<std::string>();
        };
        metadata->contractVersion = readInt("contractVersion");
        metadata->jointVocabularyVersion = readInt("jointVocabularyVersion");
        metadata->sourceFormat = readString("sourceFormat");
        metadata->sourceProvider = readString("sourceProvider");
        metadata->rootMotionSource = readString("rootMotionSource");
    }

    const pxr::VtValue source = prim.GetCustomDataByKey(kSource);
    if (source.IsHolding<pxr::VtDictionary>())
    {
        for (const auto& [key, value] : source.UncheckedGet<pxr::VtDictionary>())
        {
            if (value.IsHolding<std::string>())
            {
                metadata->provenance[key] = value.UncheckedGet<std::string>();
            }
        }
    }
}

// One channel prim the stage authored: its verbatim semantic, the attribute
// the value is read from, and the instants it was stated at.
//
// `keys` is empty for a channel stated once, without time samples, which
// applies to every sample; otherwise a sample carries the channel only where
// the stage keyed it. Not what `Get` would answer -- USD holds the last key
// forward, so every later sample would carry a value the producer never
// reported, and "an unreported name is not a value" (MOTION_CONTRACT.md §6)
// would be lost the first time a clip went through a stage.
struct StageChannel
{
    std::string name;
    pxr::UsdAttribute value;
    std::set<double> keys;
};

// The channels of §4.3, keyed on `motion:channelName` and never on the prim's
// path: a sanitized path can differ from the name, and `vrm:happy` and
// `vrm.happy` sanitize to one prim name. The writer is the half that keeps the
// paths distinct; this half never relies on them.
std::vector<StageChannel>
ReadChannels(const pxr::UsdStagePtr& stage, std::vector<std::string>* warnings)
{
    std::vector<StageChannel> channels;
    std::set<std::string> seen;
    for (const pxr::UsdPrim& prim : stage->Traverse())
    {
        const pxr::UsdAttribute nameAttr = prim.GetAttribute(kChannelName);
        if (!nameAttr)
        {
            continue;
        }
        std::string name;
        if (!nameAttr.Get(&name) || name.empty())
        {
            warnings->push_back("channel <" + prim.GetPath().GetString() +
                                "> authors no motion:channelName, so nothing can bind it");
            continue;
        }
        const pxr::UsdAttribute valueAttr = prim.GetAttribute(kChannelValue);
        // Declared and never driven is *not* a value of zero. An unreported
        // channel means the producer said nothing about it, and inventing a
        // zero here would author a statement the stage never made.
        if (!valueAttr || !valueAttr.HasValue())
        {
            continue;
        }
        if (!seen.insert(name).second)
        {
            warnings->push_back("the stage carries channel '" + name + "' more than once; <" +
                                prim.GetPath().GetString() + "> is ignored");
            continue;
        }
        std::vector<double> keys;
        valueAttr.GetTimeSamples(&keys);
        channels.push_back(StageChannel{name, valueAttr, std::set<double>(keys.begin(), keys.end())});
    }
    return channels;
}

} // namespace

std::optional<MotionPose>
PoseFromStageSample(const MotionStageSample& sample)
{
    if (!(sample.timeCodesPerSecond > 0.0))
    {
        return std::nullopt;
    }

    MotionPose pose;
    // The default time code is not frame zero, and this is the one place the
    // difference produces no wrong number: a pose resolved outside a timeline
    // carries no second, `timestamp` has no absent state, and frame zero
    // converts to 0.0 at every rate. What differs between the two is which
    // values were resolved, and that happened before this call.
    if (sample.hasTimeCode)
    {
        pose.timestamp = sample.timeCode / sample.timeCodesPerSecond;
    }

    const std::size_t jointCount = sample.jointTokens.size();
    const bool rotationsUsable = sample.rotations.size() == jointCount;
    const bool translationsUsable = sample.translations.size() == jointCount;

    for (std::size_t i = 0; i < jointCount; ++i)
    {
        const std::optional<HumanJoint> joint = JointForToken(sample.jointTokens[i]);
        if (!joint)
        {
            continue;
        }
        const auto slot = static_cast<std::size_t>(*joint);

        if (rotationsUsable)
        {
            // Normalized on the way in: a clip may author a quaternion that has
            // drifted off the unit sphere, and every consumer of a canonical
            // pose is entitled to a rotation.
            pose.localRotations[slot] = sample.rotations[i].GetNormalized();
            pose.validRotations.set(slot);
        }

        if (*joint != HumanJoint::Hips)
        {
            continue;
        }
        // The hips are the root, and the contract records both halves of it
        // (MOTION_CONTRACT.md §5.3). Their translation is body translation --
        // every other joint's is the source rig's rest offset, which a
        // retargeter re-derives per rig -- and their rotation is the body's
        // orientation *and* stays the local rotation authored above. Keeping
        // only the local rotation, as both of `usd-vrm-plugins`' readers did,
        // loses the body's facing for every consumer that reads root motion.
        if (rotationsUsable)
        {
            pose.root.worldOrientation = pose.localRotations[slot];
            pose.root.hasOrientation = true;
        }
        if (translationsUsable)
        {
            pose.root.worldPosition = sample.translations[i];
            pose.root.hasPosition = true;
        }
    }

    return pose;
}

bool
ReadMotionStage(const pxr::UsdStagePtr& stage, const std::string& skeletonPath,
                MotionStageRead* read, std::string* error)
{
    if (!stage)
    {
        *error = "no stage to read";
        return false;
    }

    *read = MotionStageRead();
    read->timeCodesPerSecond = stage->GetTimeCodesPerSecond();
    if (!(read->timeCodesPerSecond > 0.0))
    {
        read->warnings.push_back("the stage states no usable timeCodesPerSecond; reading its "
                                 "samples at " +
                                 pxr::TfStringify(MotionStageTimeCodesPerSecond));
        read->timeCodesPerSecond = MotionStageTimeCodesPerSecond;
    }

    pxr::UsdSkelSkeleton skeleton;
    if (!FindSkeleton(stage, skeletonPath, &skeleton, error))
    {
        return false;
    }
    if (!ReadSkeletonValues(skeleton, &read->skeleton, &read->warnings))
    {
        *error = "skeleton <" + skeleton.GetPath().GetString() + "> authors no joints";
        return false;
    }

    pxr::UsdPrim animationPrim;
    if (!pxr::UsdSkelBindingAPI(skeleton.GetPrim()).GetAnimationSource(&animationPrim) ||
        !animationPrim)
    {
        // A stage whose skeleton carries no binding is still usable when it
        // holds exactly one animation; two, and which one is the clip is a
        // question only the caller can answer.
        std::vector<pxr::UsdPrim> animations;
        for (const pxr::UsdPrim& prim : stage->Traverse())
        {
            if (prim.IsA<pxr::UsdSkelAnimation>())
            {
                animations.push_back(prim);
            }
        }
        if (animations.size() != 1)
        {
            *error = "skeleton <" + read->skeleton.path +
                     "> has no skel:animationSource and the stage does not hold exactly one "
                     "UsdSkelAnimation";
            return false;
        }
        animationPrim = animations.front();
    }

    const pxr::UsdSkelAnimation animation(animationPrim);
    read->animationPath = animationPrim.GetPath().GetString();

    pxr::VtTokenArray animationJoints;
    if (!animation.GetJointsAttr().Get(&animationJoints) || animationJoints.empty())
    {
        *error = "animation <" + read->animationPath + "> has no joints";
        return false;
    }

    std::vector<std::string> jointTokens;
    jointTokens.reserve(animationJoints.size());
    std::size_t recognized = 0;
    for (const pxr::TfToken& joint : animationJoints)
    {
        jointTokens.push_back(joint.GetString());
        if (JointForToken(jointTokens.back()))
        {
            ++recognized;
        }
        else
        {
            read->warnings.push_back("joint '" + jointTokens.back() +
                                     "' is not a joint of the vocabulary and was ignored");
        }
    }
    if (recognized == 0)
    {
        // §7's line: a skeleton whose tokens are not semantic paths needs a
        // RetargetMap in reverse, which is a retarget and not a read.
        *error = "no joint of animation <" + read->animationPath +
                 "> names a joint of the vocabulary; a skeleton that is not semantic reads only "
                 "through a retarget";
        return false;
    }

    // The rate the stage states twice, and the shim is not allowed to disagree
    // with the metadata it was written from (EXEC_CONTRACT.md §5.1). A reader
    // that silently preferred one would make the disagreement unobservable
    // where it matters -- an exec graph reads the attribute and this reads the
    // metadata, so the two would sample the same clip at two rates.
    double authoredRate = 0.0;
    if (const pxr::UsdAttribute rate = animationPrim.GetAttribute(kRate);
        rate && rate.Get(&authoredRate) && authoredRate != read->timeCodesPerSecond)
    {
        read->warnings.push_back("animation <" + read->animationPath + "> states motion:" +
                                 "timeCodesPerSecond = " + pxr::TfStringify(authoredRate) +
                                 " while the stage states " +
                                 pxr::TfStringify(read->timeCodesPerSecond) +
                                 "; the stage's is used");
    }

    ReadMetadata(stage->GetDefaultPrim(), &read->metadata);
    if (read->metadata.contractVersion &&
        *read->metadata.contractVersion > MotionStageContractVersion)
    {
        read->warnings.push_back(
            "the stage states motion contract version " +
            pxr::TfStringify(*read->metadata.contractVersion) + ", newer than this library's " +
            pxr::TfStringify(MotionStageContractVersion) + "; what it added is not read");
    }

    const pxr::UsdAttribute rotationsAttr = animation.GetRotationsAttr();
    const pxr::UsdAttribute translationsAttr = animation.GetTranslationsAttr();

    std::set<double> timeCodes;
    {
        std::vector<double> times;
        rotationsAttr.GetTimeSamples(&times);
        timeCodes.insert(times.begin(), times.end());
        translationsAttr.GetTimeSamples(&times);
        timeCodes.insert(times.begin(), times.end());
    }

    // A channel keys into the instants the poses already exist at, so a weight
    // that moves between two body keys has somewhere to say so.
    const std::vector<StageChannel> channels = ReadChannels(stage, &read->warnings);
    for (const StageChannel& channel : channels)
    {
        timeCodes.insert(channel.keys.begin(), channel.keys.end());
    }

    if (timeCodes.empty())
    {
        // A stage with no time sample still has default values; that is one
        // pose, at an instant the stage chose rather than one the clip stated,
        // which is what the warning says.
        timeCodes.insert(stage->GetStartTimeCode());
        read->warnings.push_back("the stage states no time sample, so its one pose is placed at "
                                 "the start time code, " +
                                 pxr::TfStringify(stage->GetStartTimeCode()));
    }

    // Scale is never animated (§4.2), and a stage that animates it is read
    // without it rather than refused: the values a reader carries are
    // rotations and translations, and a caller re-deriving scale from a clip
    // has a rig to take it from.
    {
        const pxr::UsdAttribute scalesAttr = animation.GetScalesAttr();
        std::vector<pxr::UsdTimeCode> at{pxr::UsdTimeCode::Default()};
        std::vector<double> times;
        scalesAttr.GetTimeSamples(&times);
        at.insert(at.end(), times.begin(), times.end());
        for (const pxr::UsdTimeCode when : at)
        {
            pxr::VtVec3hArray scales;
            if (!scalesAttr.Get(&scales, when))
            {
                continue;
            }
            const auto nonUnit = std::find_if(scales.begin(), scales.end(),
                                              [](const pxr::GfVec3h& scale)
                                              { return scale != pxr::GfVec3h(1.0f); });
            if (nonUnit == scales.end())
            {
                continue;
            }
            const auto index = static_cast<std::size_t>(nonUnit - scales.begin());
            read->warnings.push_back(
                "animation <" + read->animationPath + "> scales joint '" +
                (index < jointTokens.size() ? jointTokens[index] : pxr::TfStringify(index)) +
                "' to " + pxr::TfStringify(pxr::GfVec3f(*nonUnit)) +
                (when.IsDefault() ? std::string(" by default")
                                  : " at time code " + pxr::TfStringify(when.GetValue())) +
                "; scale is not carried on a pose");
            break;
        }
    }

    read->clip.samples.reserve(timeCodes.size());
    for (const double timeCode : timeCodes)
    {
        MotionStageSample sample;
        sample.jointTokens = jointTokens;
        sample.timeCode = timeCode;
        sample.hasTimeCode = true;
        sample.timeCodesPerSecond = read->timeCodesPerSecond;

        pxr::VtQuatfArray rotations;
        if (rotationsAttr.Get(&rotations, timeCode))
        {
            sample.rotations.assign(rotations.begin(), rotations.end());
        }
        pxr::VtVec3fArray translations;
        if (translationsAttr.Get(&translations, timeCode))
        {
            sample.translations.assign(translations.begin(), translations.end());
        }

        std::optional<MotionPose> pose = PoseFromStageSample(sample);
        if (!pose)
        {
            // Unreachable: the rate was made positive above. Stated rather
            // than assumed, because the only alternative is a pose with an
            // invented second.
            *error = "the stage's rate does not turn its time codes into seconds";
            return false;
        }

        for (const StageChannel& channel : channels)
        {
            if (!channel.keys.empty() && channel.keys.count(timeCode) == 0)
            {
                continue;
            }
            float value = 0.0f;
            // Carried verbatim, out-of-range values included: clamping is what
            // a consumer applying a channel to a rig does, and this is the
            // read.
            if (channel.value.Get(&value, timeCode))
            {
                pose->channels.Set(channel.name, value);
            }
        }

        read->clip.samples.push_back(std::move(*pose));
    }

    read->clip.startTime = read->clip.samples.front().timestamp;
    read->clip.endTime = read->clip.samples.back().timestamp;
    read->clip.nominalFrameRate = read->timeCodesPerSecond;
    read->clip.source.kind = MotionSourceKind::Clip;
    read->clip.source.provider = read->metadata.sourceProvider;
    read->clip.source.sourceId = stage->GetRootLayer() ? stage->GetRootLayer()->GetIdentifier()
                                                       : std::string();
    return true;
}

bool
OpenMotionStage(const std::string& path, const std::string& skeletonPath, MotionStageRead* read,
                std::string* error)
{
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(path);
    if (stage)
    {
        return ReadMotionStage(stage, skeletonPath, read, error);
    }

    // Asked only after `Open` refused, never before it: a path the resolver
    // understands and the file system does not -- a package-relative path, a
    // URI -- opens without reaching here, so this cannot refuse a stage
    // OpenUSD would have opened.
    //
    // "There" means a regular file, not any path: a directory exists and is
    // still not a layer, and letting it through would report a missing file
    // format for a mistyped argument. Symlinks are followed.
    if (pxr::TfIsDir(path, /* resolveSymlinks = */ true))
    {
        *error = path + " is a directory, not a motion stage";
    }
    else if (!pxr::TfIsFile(path, /* resolveSymlinks = */ true))
    {
        *error = "no file at " + path;
    }
    else
    {
        // The file is there and OpenUSD would not open it, which splits two
        // ways this library cannot tell apart: it is not a layer, or its
        // format needs a plugin that is not registered. Asking the format
        // registry which would answer it, and a converter that is not a file
        // format plugin does not reach for the file format API
        // (USD_MAPPING.md §1, the boundary check).
        *error = "OpenUSD could not open the stage " + path +
                 "; it is not a layer, or its file format plugin is not registered";
    }
    return false;
}

} // namespace openstrata::motion
