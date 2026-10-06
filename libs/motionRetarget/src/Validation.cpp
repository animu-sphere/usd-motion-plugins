// SPDX-License-Identifier: Apache-2.0
#include "motionRetarget/Validation.h"

#include <cmath>
#include <set>

namespace openstrata::motion {
namespace {
// Linear-time cycle detection, without recursive traversal or ancestor calls
// that assume a valid hierarchy. Out-of-range parents are checked separately.
void
CheckCycles(const std::vector<int>& parents, ValidationReport& report,
            const std::vector<std::string>& subjects)
{
    std::vector<unsigned char> state(parents.size(), 0);
    for (std::size_t start = 0; start < parents.size(); ++start) {
        if (state[start])
            continue;
        std::vector<std::size_t> path;
        int current = static_cast<int>(start);
        while (current >= 0 && static_cast<std::size_t>(current) < parents.size() &&
               !state[current]) {
            state[current] = 1;
            path.push_back(static_cast<std::size_t>(current));
            current = parents[current];
        }
        if (current >= 0 && static_cast<std::size_t>(current) < parents.size() &&
            state[current] == 1) {
            bool inCycle = false;
            for (const auto index : path) {
                inCycle = inCycle || index == static_cast<std::size_t>(current);
                if (inCycle)
                    report.Report(ValidationCode::HierarchyCycle,
                                  subjects[index],
                                  "Parent hierarchy contains a cycle");
            }
        }
        for (const auto index : path)
            state[index] = 2;
    }
}
} // namespace

ValidationReport
ValidateSkeletonDescriptor(const SkeletonDescriptor& skeleton, QuaternionValidationPolicy policy)
{
    ValidationReport report;
    report.Merge(ValidateQuaternion(pxr::GfQuatf(1.0f), "", policy));
    std::set<std::string> tokens;
    std::vector<int> parents;
    std::vector<std::string> subjects;
    const auto& joints = skeleton.GetJoints();
    for (std::size_t i = 0; i < joints.size(); ++i) {
        const auto& joint = joints[i];
        const std::string subject = "joints[" + std::to_string(i) + "]";
        parents.push_back(joint.parent);
        subjects.push_back(subject + ".parent");
        if (joint.token.empty())
            report.Report(
                ValidationCode::EmptyJointToken, subject + ".token", "Joint must have a token");
        if (!tokens.insert(joint.token).second)
            report.Report(ValidationCode::DuplicateJointToken,
                          subject + ".token",
                          "Joint tokens must be unique");
        if (joint.parent != SkeletonDescriptor::kNoParent) {
            if (joint.parent < 0 || static_cast<std::size_t>(joint.parent) >= joints.size())
                report.Report(ValidationCode::InvalidParent,
                              subject + ".parent",
                              "Parent index is outside the skeleton");
            else if (static_cast<std::size_t>(joint.parent) >= i)
                report.Report(ValidationCode::HierarchyOrder,
                              subject + ".parent",
                              "Parent must precede its child");
        }
        report.Merge(ValidateQuaternion(joint.restRotation, subject + ".restRotation", policy));
        report.Merge(ValidateVector(joint.restTranslation, subject + ".restTranslation"));
        report.Merge(ValidateVector(joint.restScale, subject + ".restScale"));
    }
    CheckCycles(parents, report, subjects);
    return report;
}

ValidationReport
ValidateSourceRestPose(const SourceRestPose& rest, QuaternionValidationPolicy policy)
{
    ValidationReport report;
    std::vector<int> parents;
    std::vector<std::string> subjects;
    for (std::size_t i = 0; i < HumanJointCount; ++i) {
        const std::string bone(HumanJointName(static_cast<HumanJoint>(i)));
        report.Merge(ValidateQuaternion(rest.localRotations[i], "localRotations." + bone, policy));
        report.Merge(ValidateVector(rest.localTranslations[i], "localTranslations." + bone));
        const std::string subject = "parents." + bone;
        subjects.push_back(subject);
        const auto parent = rest.parents[i];
        // Check before narrowing arbitrary public size_t values.
        parents.push_back(parent < HumanJointCount ? static_cast<int>(parent) : -1);
        if (parent > SourceRestPose::kNoParent)
            report.Report(
                ValidationCode::InvalidParent, subject, "Parent is outside the joint vocabulary");
    }
    CheckCycles(parents, report, subjects);
    return report;
}

RetargetValidationReport
ValidateRetargetConfiguration(const SkeletonDescriptor& skeleton, const RetargetMap& map,
                              const SourceRestPose& sourceRest, const RetargetOptions& options,
                              QuaternionValidationPolicy policy)
{
    RetargetValidationReport report;
    report.values.Merge(ValidateSkeletonDescriptor(skeleton, policy), "skeleton.");
    report.values.Merge(ValidateSourceRestPose(sourceRest, policy), "sourceRest.");
    for (std::size_t i = 0; i < HumanJointCount; ++i) {
        const auto bone = static_cast<HumanJoint>(i);
        const int index = map.GetJointIndex(bone);
        if (index != RetargetMap::kUnmapped &&
            (index < 0 || static_cast<std::size_t>(index) >= skeleton.GetSize()))
            report.values.Report(ValidationCode::InvalidJointIndex,
                                 "map." + std::string(HumanJointName(bone)),
                                 "Mapped joint is outside this skeleton");
    }
    if (options.targetRest.localRotations.size() > skeleton.GetSize())
        report.values.Report(ValidationCode::TargetRestSize,
                             "options.targetRest.localRotations",
                             "Target reference rest exceeds the skeleton");
    for (std::size_t i = 0; i < options.targetRest.localRotations.size(); ++i)
        if (options.targetRest.localRotations[i])
            report.values.Merge(
                ValidateQuaternion(*options.targetRest.localRotations[i],
                                   "options.targetRest.localRotations[" + std::to_string(i) + "]",
                                   policy));
    const auto mode = options.rootMotion.mode;
    if (mode != RootMotionMode::Ignore && mode != RootMotionMode::Hips &&
        mode != RootMotionMode::RootJoint)
        report.values.Report(
            ValidationCode::InvalidEnum, "options.rootMotion.mode", "Unknown root-motion mode");
    if (!std::isfinite(options.rootMotion.translationScale))
        report.values.Report(ValidationCode::NonFiniteValue,
                             "options.rootMotion.translationScale",
                             "Root translation scale must be finite");
    for (std::size_t i = 0; i < options.requiredBones.size(); ++i)
        if (!IsValidHumanJoint(options.requiredBones[i]))
            report.values.Report(ValidationCode::InvalidEnum,
                                 "options.requiredBones[" + std::to_string(i) + "]",
                                 "Required bone is outside the joint vocabulary");
    report.diagnostics = DiagnoseRig(skeleton, map, options);
    return report;
}
} // namespace openstrata::motion
