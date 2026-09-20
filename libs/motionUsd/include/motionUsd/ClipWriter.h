// SPDX-License-Identifier: Apache-2.0
//
// The standalone motion stage (docs/design/USD_MAPPING.md §2-§5): a
// `MotionClip` authored as standard UsdSkel, with nothing that names a target
// rig. This is the stage a recorded, converted or generated clip becomes, and
// a retarget reads it without knowing which of those it was.
//
// motionUsd converts; it is not a file-format plugin (USD_MAPPING.md §1). A
// format plugin that authors a motion stage calls `AuthorMotionStage` on the
// stage it is filling.
#pragma once

#include "motionUsd/api.h"

#include "motionCore/MotionPose.h"

#include "pxr/usd/usd/stage.h"

#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec3f.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace openstrata::motion
{

// `customData.motion.contractVersion` on every stage this library authors
// (USD_MAPPING.md §8).
inline constexpr int MotionStageContractVersion = 1;

// Always 30, whatever rate the samples were taken at (USD_MAPPING.md §4.1,
// USD-O2). The samples keep their own times; this is only where they are
// written.
inline constexpr double MotionStageTimeCodesPerSecond = 30.0;

// A producer's rest pose, joint by joint, for a clip whose rest is not
// identity (USD_MAPPING.md §3): a recorded file states one, and a profile says
// how to read it. Rotations default to identity and translations to zero, so a
// caller fills only the joints it carries and sets them in `present`.
//
// It is the rest a retargeter corrects from. Authoring identity for a file
// that stated otherwise would tell the retargeter that the source rig stands
// exactly as the target does, and skip the correction without a word.
struct MotionStageRest
{
    MotionStageRest()
    {
        localRotations.fill(pxr::GfQuatf(1.0f));
        localTranslations.fill(pxr::GfVec3f(0.0f));
    }

    std::array<pxr::GfQuatf, HumanJointCount> localRotations;
    std::array<pxr::GfVec3f, HumanJointCount> localTranslations;
    std::bitset<HumanJointCount> present;
};

// What the caller knows about the clip that the clip does not carry.
struct MotionStageOptions
{
    // Authored into `customData.motion` under the key of the same name
    // (USD_MAPPING.md §5), and left out when empty. How the motion arrived:
    // `bvh`, `capture`, ...
    std::string sourceFormat;
    // Which producer channel became the hips translation.
    std::string rootMotionSource;

    // The producer's rest. Absent for a clip whose rotations are relative to
    // the canonical rest (a capture): the rest is then identity, except the
    // hips translation at the first root position.
    std::optional<MotionStageRest> rest;

    // A recorded source's provenance, authored verbatim as the
    // `customData.source` dictionary: the fields `SourceMetadata` narrows away
    // survive here, beside the motion (MOTION_CONTRACT.md §7.1). Nothing reads
    // them to decide anything.
    std::map<std::string, std::string> provenance;
};

// What the stage holds, and what the clip carried that it does not.
struct MotionStageReport
{
    std::size_t jointCount = 0;
    std::size_t sampleCount = 0;

    // Channel names the clip carried, sorted, each once. The prim's shape is
    // decided (USD_MAPPING.md §4.3) but nothing authors it until the reading
    // half arrives, so a caller that needs them has to be told they were
    // dropped.
    std::vector<std::string> unauthoredChannels;

    // Samples whose look-at target was not authored. A target is a point in
    // the root's space, and the mapping gives it no place yet.
    std::size_t unauthoredLookAtTargets = 0;
};

// Authors `clip` into `stage`'s root layer:
//
//     /Animation            Scope, the default prim; customData.motion
//       /Skeleton           UsdSkelSkeleton over semantic joint paths
//       /Body               UsdSkelAnimation, bound to /Animation/Skeleton
//
// with `upAxis = Y`, `metersPerUnit = 1` and 30 time codes per second, which
// `Body` also states as `motion:timeCodesPerSecond` (EXEC_CONTRACT.md §5.1).
//
// - The joint set. Without a producer rest, a joint is on the skeleton when
//   any sample observed it, and the hips also when any sample carried a root
//   position; a joint never observed is absent, not authored at rest. With a
//   producer rest it is `rest.present`, every joint the producer's rig carries,
//   whether or not a sample turned it: two recordings of one rig must author
//   one skeleton.
// - Rests are the producer's, or, without one, identity except the hips
//   translation at the first observed root position (USD_MAPPING.md §3).
// - `Body` authors, for every joint:
//   - its rotation, or its rest rotation where a sample did not observe it.
//     Holding a previous rotation is an intake policy, not this writer's.
//   - its rest translation, except the hips, which carry the root position.
//     A sample with no root position holds the last one, starting at the
//     rest.
//   It also authors a constant identity `scales` array, without which UsdSkel
//   resolves no joint transform at all (USD_MAPPING.md §4.2).
// - A sample at `t` seconds is authored at `t * 30`, snapped to the nearest
//   whole frame when within 1e-6 of it, so a clip taken at a divisor of 30 Hz
//   lands on whole frames rather than on `62.00000000000001`.
//
// Refused, with nothing authored, when:
// - the stage already holds `/Animation`;
// - the clip has no sample, or observes no joint and no root;
// - a timestamp is not finite or does not increase;
// - a producer rest carries no hips, or a sample observes a joint the rest does
//   not carry.
//
// `report` may be null.
MOTIONUSD_API bool AuthorMotionStage(const pxr::UsdStagePtr& stage, const MotionClip& clip,
                                     const MotionStageOptions& options,
                                     MotionStageReport* report, std::string* error);

// `AuthorMotionStage` into the layer at `path`, which it creates or replaces,
// then saves. A refused clip leaves `path` as it was: no file is created and
// an existing one is not touched.
MOTIONUSD_API bool WriteMotionStage(const std::string& path, const MotionClip& clip,
                                    const MotionStageOptions& options,
                                    MotionStageReport* report, std::string* error);

} // namespace openstrata::motion
