// SPDX-License-Identifier: Apache-2.0
//
// The reading half of the standalone motion stage (USD_MAPPING.md §7): a
// `UsdSkelAnimation` over semantic joint tokens becomes a `MotionClip`.
//
// A skeleton whose joint tokens are semantic paths reads directly; any other
// skeleton needs a `RetargetMap` in reverse and is a retarget, not a read, so
// it is refused here rather than guessed at.
//
// This is the library home the sampling finding asked for. `usd-vrm-plugins`
// read a clip in a CLI (`motion_retarget`'s `StageIo`, which this arrived
// from) and copied the rule line for line into an OpenExec bundle, because a
// bundle cannot call a CLI. `PoseFromStageSample` is that rule, taking values
// rather than a prim so that a caller holding a stage and one holding an
// exec computation's already-resolved inputs share it.
#pragma once

#include "motionUsd/MotionStage.h"
#include "motionUsd/api.h"

#include "motionCore/MotionPose.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/usd/usd/stage.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace openstrata::motion
{

// What a skeleton prim states, as plain values.
//
// Values and not a `SkeletonDescriptor`: that type is `motionRetarget`'s, and
// this library depends on `motionCore` alone among the retarget's neighbours
// (WORKSPACE.md §2.1). These two arrays are exactly what
// `BuildSkeletonDescriptor` takes, and its descriptor is what
// `BuildSourceRestPose` takes after it, so a caller that wants either hands
// this over unchanged and a caller that wants neither does not link a
// retargeter to read a stage.
struct MotionStageSkeleton
{
    // The skeleton prim the clip was read against.
    std::string path;

    // `joints`, in the order the skeleton authored them, and `restTransforms`
    // of the same length.
    std::vector<std::string> jointTokens;
    std::vector<pxr::GfMatrix4d> restTransforms;

    // False when the skeleton authored no usable `restTransforms` and
    // `restTransforms` above is identity per joint. `bindTransforms` are
    // world-space and cannot substitute without the topology walk, so the
    // fallback is stated rather than guessed at.
    bool restTransformsAuthored = false;
};

// `/Animation.customData.motion` (USD_MAPPING.md §5), read back.
//
// Every field is optional, because this reader also reads a stage no writer of
// this library authored — `usd-vrm-plugins`' `.vrma` stage is standard
// `UsdSkel` over the same joint tokens and carries none of this. An absent
// `contractVersion` says the stage did not claim this mapping, which is a fact
// about the stage and not a defect in it.
struct MotionStageMetadata
{
    std::optional<int> contractVersion;
    std::optional<int> jointVocabularyVersion;
    std::string sourceFormat;
    std::string sourceProvider;
    std::string rootMotionSource;

    // The producer's rate, which is not the stage's: a 60 Hz capture is
    // authored at 30 time codes per second (§4.1, USD-O2) and says so here.
    // Without it a read would answer 30, and a consumer would take the
    // encoding for the measurement.
    std::optional<double> nominalFrameRate;

    // `customData.source`, verbatim (MOTION_CONTRACT.md §7.1). Nothing here
    // reads it to decide anything.
    std::map<std::string, std::string> provenance;
};

// What a read produced.
struct MotionStageRead
{
    MotionClip clip;
    MotionStageSkeleton skeleton;

    // The `UsdSkelAnimation` prim the samples came from.
    std::string animationPath;

    // The stage's, of which `MotionStageTimeCodesPerSecond` is only the rate
    // this library *writes*: a stage authored elsewhere may state another, and
    // the samples' seconds are this number's quotient either way. It is where
    // the samples were written, never the rate they were taken at — that is
    // `metadata.nominalFrameRate`, and `clip.nominalFrameRate` carries it.
    double timeCodesPerSecond = MotionStageTimeCodesPerSecond;

    MotionStageMetadata metadata;

    // What the stage said that the clip could not carry, or said oddly. A
    // warning is never a refusal: the clip beside it is usable.
    std::vector<std::string> warnings;
};

// What a `UsdSkelAnimation` states at one instant, as plain values.
//
// `rotations` and `translations` are already resolved **at** `timeCode`. This
// converts and interpolates nothing: a stage reader resolves a time sample and
// an OpenExec node is handed one, and both then apply the same rule.
struct MotionStageSample
{
    // `UsdSkelAnimation`'s `joints`, in the order the clip authored them.
    std::vector<std::string> jointTokens;

    // `rotations` and `translations` at `timeCode`. An array whose length
    // disagrees with `jointTokens` contributes nothing, because a clip that
    // cannot say which joint a value belongs to has not said it.
    std::vector<pxr::GfQuatf> rotations;
    std::vector<pxr::GfVec3f> translations;

    // The frame these values were resolved at. `hasTimeCode` is false for the
    // **default** time code, which is what an exec system evaluates at until
    // its time is set, so it is the common case rather than an edge one.
    double timeCode = 0.0;
    bool hasTimeCode = false;

    // The rate that turns `timeCode` into the seconds `MotionPose::timestamp`
    // is expressed in.
    double timeCodesPerSecond = 0.0;
};

// The pose `sample` states, or nullopt when it cannot be stamped.
//
// Refused for a non-positive `timeCodesPerSecond`, which covers both an absent
// rate and a nonsense one. `MotionPose::timestamp` is a plain double with no
// absent state, so a pose produced without a rate would carry a second every
// consumer downstream would take at face value.
//
// Everything else is a partial answer rather than a refusal, because a clip is
// allowed to be sparse: a joint token naming no joint of the vocabulary
// contributes nothing, an array whose length disagrees with `jointTokens`
// contributes nothing, and a clip that authors no translations produces a pose
// with no root position.
//
// The hips are read twice, which is the contract's rule and not a duplication
// this function invented (MOTION_CONTRACT.md §5.3): their translation is
// `root.worldPosition` and their rotation is both `localRotations[Hips]` and
// `root.worldOrientation`. `usd-vrm-plugins` dropped the orientation in both
// of its copies, so a clip read there lost the body's facing and kept only its
// place.
//
// **Except for a clip of exactly one joint**: an unauthored `rotations` or
// `translations` reaches an OpenExec callback as one element of Sdf's fallback
// rather than as nothing, and against one joint that element pairs. From
// inside this function the fallback and an authored origin are the same value.
// A stage reader does not hit it, because it asks the attribute whether it has
// a value.
MOTIONUSD_API std::optional<MotionPose> PoseFromStageSample(const MotionStageSample& sample);

// Reads the motion `stage` holds into `read`.
//
// `skeletonPath` may be empty, and is then the first `UsdSkelSkeleton` in
// stage order; a stage carrying several is why a caller can name one. The
// animation is the skeleton's `skel:animationSource`, or the stage's one
// `UsdSkelAnimation` when the skeleton binds none.
//
// The samples are the union of the key times of `rotations`, `translations`
// and every channel — a channel keys into the instants the poses already exist
// at, so a weight that moves between two body keys has somewhere to say so. A
// clip that states no time sample at all is one pose at the stage's start time
// code, and says so in `warnings`.
//
// Refused, with `read` left unspecified, when the stage holds no skeleton, the
// named path is not one, no animation can be found, the animation authors no
// joints, or no joint token names a joint of the vocabulary — the last being
// the "not a semantic skeleton" case §7 calls a retarget rather than a read.
MOTIONUSD_API bool ReadMotionStage(const pxr::UsdStagePtr& stage, const std::string& skeletonPath,
                                   MotionStageRead* read, std::string* error);

// `ReadMotionStage` over the layer at `path`, opened through OpenUSD.
//
// A path that did not open is reported as what is wrong with it — not a file,
// a directory, an extension no file format plugin claims — rather than as one
// unattributable failure, because only the first of those is the caller's
// typing.
MOTIONUSD_API bool OpenMotionStage(const std::string& path, const std::string& skeletonPath,
                                   MotionStageRead* read, std::string* error);

} // namespace openstrata::motion
