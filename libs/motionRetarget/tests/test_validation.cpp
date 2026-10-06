// SPDX-License-Identifier: Apache-2.0
#include "motionRetarget/Validation.h"

#include <cassert>
#include <limits>

using namespace openstrata::motion;

bool
Has(const ValidationReport& report, ValidationCode code, std::string_view subject)
{
    for (const auto& d : report.reported)
        if (d.code == code && d.subject == subject)
            return true;
    return false;
}

int
main()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    SourceRestPose rest;
    assert(ValidateSourceRestPose(rest).IsValid());
    assert(ValidateSkeletonDescriptor({}).IsValid());
    SkeletonJoint root;
    root.token = "Root";
    SkeletonJoint child;
    child.token = "Root/Child";
    child.parent = 0;
    child.restScale = pxr::GfVec3f(2.0f); // Static scale is legal.
    SkeletonDescriptor skeleton({root, child});
    assert(ValidateSkeletonDescriptor(skeleton).IsValid());
    RetargetMap map;
    RetargetOptions options;
    options.rootMotion.mode = RootMotionMode::Ignore;
    auto config = ValidateRetargetConfiguration(skeleton, map, rest, options);
    assert(config.IsValid() && config.diagnostics.reported.empty());
    options.requiredBones = {HumanJoint::Head};
    config = ValidateRetargetConfiguration(skeleton, map, rest, options);
    assert(config.IsValid());
    assert(config.diagnostics == DiagnoseRig(skeleton, map, options));
    assert(config.diagnostics.Subjects(RetargetDiagnosticCode::MissingRequiredBone) ==
           std::vector<std::string>{"head"});
    map.SetJointIndex(HumanJoint::Hips, 0, skeleton.GetSize());
    map.SetJointIndex(HumanJoint::Head, 0, skeleton.GetSize());
    config = ValidateRetargetConfiguration(skeleton, map, rest, options);
    assert(config.IsValid());
    assert(config.diagnostics == DiagnoseRig(skeleton, map, options));
    assert(config.diagnostics.Subjects(RetargetDiagnosticCode::DuplicateTarget) ==
           std::vector<std::string>{"Root"});
    options.rootMotion.mode = RootMotionMode::RootJoint;
    options.rootMotion.rootJointIndex = 12;
    config = ValidateRetargetConfiguration(skeleton, map, rest, options);
    assert(config.IsValid() && config.diagnostics == DiagnoseRig(skeleton, map, options));
    assert(!config.diagnostics.Subjects(RetargetDiagnosticCode::InvalidRootJoint).empty());

    auto malformed = child;
    malformed.token = "Root";
    malformed.parent = 2;
    malformed.restRotation = pxr::GfQuatf(0.0f);
    malformed.restTranslation[0] = nan;
    malformed.restScale[1] = nan;
    auto report = ValidateSkeletonDescriptor(SkeletonDescriptor({root, malformed}));
    assert(Has(report, ValidationCode::DuplicateJointToken, "joints[1].token"));
    assert(Has(report, ValidationCode::InvalidParent, "joints[1].parent"));
    assert(Has(report, ValidationCode::InvalidQuaternion, "joints[1].restRotation"));
    assert(Has(report, ValidationCode::NonFiniteValue, "joints[1].restTranslation"));
    assert(Has(report, ValidationCode::NonFiniteValue, "joints[1].restScale"));
    malformed.token.clear();
    malformed.parent = -2;
    report = ValidateSkeletonDescriptor(SkeletonDescriptor({root, malformed}));
    assert(Has(report, ValidationCode::EmptyJointToken, "joints[1].token"));
    assert(Has(report, ValidationCode::InvalidParent, "joints[1].parent"));
    auto cyclicRoot = root;
    cyclicRoot.parent = 1;
    report = ValidateSkeletonDescriptor(SkeletonDescriptor({cyclicRoot, child}));
    assert(Has(report, ValidationCode::HierarchyOrder, "joints[0].parent"));
    assert(Has(report, ValidationCode::HierarchyCycle, "joints[0].parent"));
    assert(Has(report, ValidationCode::HierarchyCycle, "joints[1].parent"));
    child.parent = 1;
    assert(Has(ValidateSkeletonDescriptor(SkeletonDescriptor({root, child})),
               ValidationCode::HierarchyCycle,
               "joints[1].parent"));

    // Vocabulary order does not define source rest topology (eyes precede arms).
    rest.parents[0] = 20;
    assert(ValidateSourceRestPose(rest).IsValid());
    rest.parents[20] = 0;
    report = ValidateSourceRestPose(rest);
    assert(Has(report, ValidationCode::HierarchyCycle, "parents.hips"));
    assert(Has(report, ValidationCode::HierarchyCycle, "parents.leftHand"));
    rest.parents[20] = std::numeric_limits<std::size_t>::max();
    assert(Has(ValidateSourceRestPose(rest), ValidationCode::InvalidParent, "parents.leftHand"));
    rest.parents[20] = SourceRestPose::kNoParent;
    rest.localRotations[0] = pxr::GfQuatf(2.0f);
    rest.localTranslations[0][0] = nan;
    report = ValidateSourceRestPose(rest);
    assert(Has(report, ValidationCode::NonUnitQuaternion, "localRotations.hips"));
    assert(Has(report, ValidationCode::NonFiniteValue, "localTranslations.hips"));
    rest.localTranslations[0] = pxr::GfVec3f(0.0f);
    assert(ValidateSourceRestPose(rest, QuaternionValidationPolicy::Normalizable).IsValid());

    rest = SourceRestPose{};
    options = RetargetOptions{};
    // A map can have been built against a different, larger rig.
    map.SetJointIndex(HumanJoint::Head, 5, 6);
    options.targetRest.localRotations = {std::nullopt, pxr::GfQuatf(nan), pxr::GfQuatf(1.0f)};
    options.rootMotion.mode = static_cast<RootMotionMode>(255);
    options.rootMotion.translationScale = nan;
    options.requiredBones = {HumanJoint::Count};
    config = ValidateRetargetConfiguration(skeleton, map, rest, options);
    assert(!config.IsValid());
    assert(Has(config.values, ValidationCode::InvalidJointIndex, "map.head"));
    assert(Has(config.values, ValidationCode::TargetRestSize, "options.targetRest.localRotations"));
    assert(Has(
        config.values, ValidationCode::InvalidQuaternion, "options.targetRest.localRotations[1]"));
    assert(Has(config.values, ValidationCode::InvalidEnum, "options.rootMotion.mode"));
    assert(
        Has(config.values, ValidationCode::NonFiniteValue, "options.rootMotion.translationScale"));
    assert(Has(config.values, ValidationCode::InvalidEnum, "options.requiredBones[0]"));
    assert(config.diagnostics == DiagnoseRig(skeleton, map, options));
    options = RetargetOptions{};
    options.targetRest.localRotations = {std::nullopt};
    map.Clear();
    assert(ValidateRetargetConfiguration(skeleton, map, rest, options).IsValid());
}
