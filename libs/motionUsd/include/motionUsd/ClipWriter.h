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

#include <cstddef>
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

// What the caller knows about the clip that the clip does not carry. Each
// string is authored into `customData.motion` under the key of the same name
// (USD_MAPPING.md §5), and left out when empty.
struct MotionStageOptions
{
    // How the motion arrived: `bvh`, `capture`, ...
    std::string sourceFormat;
    // Which producer channel became the hips translation.
    std::string rootMotionSource;
};

// What the stage holds, and what the clip carried that it does not.
struct MotionStageReport
{
    std::size_t jointCount = 0;
    std::size_t sampleCount = 0;

    // Channel names the clip carried, sorted, each once. `/Animation/Channels`
    // is not authored until its attribute names are decided (USD-O4), so a
    // caller that needs them has to be told they were dropped.
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
// - A joint is on the skeleton when any sample observed it, and the hips also
//   when any sample carried a root position. A joint never observed is absent,
//   not authored at rest.
// - Rests are identity except the hips translation, which is the first
//   observed root position (USD_MAPPING.md §3).
// - `Body` authors rotations for every joint (identity where a sample did not
//   observe one: holding is an intake policy, not this writer's), the root
//   position as the hips translation (held from the last sample that carried
//   one, starting at the rest), zero for every other translation, and a
//   constant identity `scales` array, without which UsdSkel resolves no joint
//   transform at all (USD_MAPPING.md §4.2).
// - A sample at `t` seconds is authored at `t * 30`, snapped to the nearest
//   whole frame when within 1e-6 of it, so a clip taken at a divisor of 30 Hz
//   lands on whole frames rather than on `62.00000000000001`.
//
// Refused, with nothing authored, when the clip has no sample, observes no
// joint and no root, or has a timestamp that is not finite or does not
// increase. `report` may be null.
MOTIONUSD_API bool AuthorMotionStage(const pxr::UsdStagePtr& stage, const MotionClip& clip,
                                     const MotionStageOptions& options,
                                     MotionStageReport* report, std::string* error);

// `AuthorMotionStage` into the layer at `path`, created or cleared, then saved.
MOTIONUSD_API bool WriteMotionStage(const std::string& path, const MotionClip& clip,
                                    const MotionStageOptions& options,
                                    MotionStageReport* report, std::string* error);

} // namespace openstrata::motion
