// SPDX-License-Identifier: Apache-2.0
//
// The stage half of the convert tool: a converted clip and its rest become the
// standalone motion stage, through motionUsd (USD_MAPPING.md §2-§5).
//
// In usd-vrm-plugins this was the third copy of one shape: the capture tool,
// this converter and the `.vrma` importer each authored a semantic clip, and
// this file's header argued they stay repeated until a fourth caller made the
// difference between them a parameter. The difference was the rest pose: a
// capture's is identity, and a recorded file states one. motionUsd is that
// function, and the rest is its parameter (`MotionStageOptions::rest`). What
// stays here is the mapping from this tool's values onto it.
#pragma once

#include "motionSource/CanonicalConversion.h"

#include "motionCore/MotionPose.h"

#include <map>
#include <string>

namespace motionConvertTool
{

// Writes `animation` over `rest` as a motion stage at `outputPath`.
//
// `rootJoint` is the source joint the profile names as the root, whose
// translation became the hips'. It is recorded as the stage's
// `rootMotionSource`.
//
// The joint set is `rest.present`, every bone the profile bound, and not the
// bones some frame happened to rotate.
//
// `provenance` is written verbatim as the stage's `customData.source`, so a
// baked result can be traced back to the file and the profile that produced
// it. Neither this function nor its caller may branch on any of it: a producer
// name reaching an `if` is the failure the profile design exists to prevent.
bool WriteSemanticClip(const std::string& outputPath, const openstrata::motion::MotionClip& animation,
                       const openstrata::motion::CanonicalRestPose& rest,
                       const std::string& rootJoint,
                       const std::map<std::string, std::string>& provenance, std::string* error);

} // namespace motionConvertTool
