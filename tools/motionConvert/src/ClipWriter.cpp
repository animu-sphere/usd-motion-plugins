// SPDX-License-Identifier: Apache-2.0
#include "ClipWriter.h"

#include "motionUsd/ClipWriter.h"

namespace motionConvertTool
{

bool
WriteSemanticClip(const std::string& outputPath, const openstrata::motion::MotionClip& animation,
                  const openstrata::motion::CanonicalRestPose& rest, const std::string& rootJoint,
                  const std::map<std::string, std::string>& provenance, std::string* error)
{
    if (animation.samples.empty())
    {
        *error = "the conversion produced no frames";
        return false;
    }
    if (!rest.present.any())
    {
        *error = "the conversion bound no humanoid bone";
        return false;
    }

    // The rest the converter built, joint for joint. This is the whole of what
    // separates a recorded clip from a capture: the file states a rest and the
    // profile says how to read it, so leaving it out would tell a retargeter
    // that the source rig stands exactly as the target does and silently skip
    // the correction.
    openstrata::motion::MotionStageRest stageRest;
    stageRest.localRotations = rest.localRotations;
    stageRest.localTranslations = rest.localTranslations;
    stageRest.present = rest.present;

    openstrata::motion::MotionStageOptions options;
    const auto format = provenance.find("format");
    if (format != provenance.end())
    {
        options.sourceFormat = format->second;
    }
    // The converter puts the profile's root joint's translation, read under
    // the profile's root policy, on the hips (CanonicalConversion.h).
    options.rootMotionSource = rootJoint + " translation";
    options.rest = std::move(stageRest);
    options.provenance = provenance;

    // The hips are guaranteed by `ValidateSourceProfile`, which requires every
    // profile to bind them; motionUsd checks it again rather than authoring
    // body translation onto a joint that is not in the joint set.
    return openstrata::motion::WriteMotionStage(outputPath, animation, options, nullptr, error);
}

} // namespace motionConvertTool
