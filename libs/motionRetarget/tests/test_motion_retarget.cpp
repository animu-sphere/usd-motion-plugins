// SPDX-License-Identifier: Apache-2.0
#include "motionRetarget/PoseRetargeter.h"
#include "motionRetarget/RestPose.h"
#include "motionRetarget/RetargetMap.h"
#include "motionRetarget/RootMotionPolicy.h"
#include "motionRetarget/SkeletonDescriptor.h"

#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/rotation.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr float kEpsilon = 1e-4f;

bool
NearlyEqual(float a, float b)
{
    return std::fabs(a - b) <= kEpsilon;
}

bool
NearlyEqual(const pxr::GfVec3f& a, const pxr::GfVec3f& b)
{
    return NearlyEqual(a[0], b[0]) && NearlyEqual(a[1], b[1]) && NearlyEqual(a[2], b[2]);
}

// Compares orientations, not representations: q and -q are the same rotation.
bool
SameOrientation(const pxr::GfQuatf& a, const pxr::GfQuatf& b)
{
    const pxr::GfQuatf na = a.GetNormalized();
    pxr::GfQuatf nb = b.GetNormalized();
    if (pxr::GfDot(na, nb) < 0.0f)
    {
        nb = pxr::GfQuatf(-nb.GetReal(), -nb.GetImaginary());
    }
    return NearlyEqual(na.GetReal(), nb.GetReal()) &&
           NearlyEqual(na.GetImaginary()[0], nb.GetImaginary()[0]) &&
           NearlyEqual(na.GetImaginary()[1], nb.GetImaginary()[1]) &&
           NearlyEqual(na.GetImaginary()[2], nb.GetImaginary()[2]);
}

pxr::GfQuatf
Rotation(const pxr::GfVec3f& axis, float degrees)
{
    const float radians = degrees * 3.14159265358979324f / 180.0f;
    return pxr::GfQuatf(std::cos(radians * 0.5f), axis.GetNormalized() * std::sin(radians * 0.5f));
}

const pxr::GfVec3f kAxisX(1.0f, 0.0f, 0.0f);
const pxr::GfVec3f kAxisY(0.0f, 1.0f, 0.0f);
const pxr::GfVec3f kAxisZ(0.0f, 0.0f, 1.0f);

// The design triplet's target rig (docs/design/fixtures/motion/avatar.usda):
// Root, Pelvis, SpineA, ChestA with translation-only rest transforms. The
// fixture lists them as flat sibling tokens; here they carry the hierarchical
// "a/b/c" joint paths so parent resolution is exercised too. Both spellings are
// valid UsdSkelSkeleton.joints, and the end-to-end test covers the flat one.
// The one rest decomposition `motion_retarget` and `execVrm` share: a scale
// kept apart from the rotation rather than folded into it, and part of a
// joint's identity (the scale policy).
void
TestARestTransformDecomposesItsScale()
{
    const pxr::GfQuatf turn = Rotation(kAxisZ, 90.0f);
    pxr::GfMatrix4d rest;
    rest.SetScale(pxr::GfVec3d(2.0, 3.0, 0.5));
    rest *= pxr::GfMatrix4d().SetRotate(
        pxr::GfQuatd(turn.GetReal(), pxr::GfVec3d(turn.GetImaginary())));
    rest *= pxr::GfMatrix4d().SetTranslate(pxr::GfVec3d(0.1, 0.2, 0.3));

    openstrata::motion::SkeletonJoint joint;
    joint.token = "Arm";
    joint.parent = 4;
    openstrata::motion::DecomposeRestTransform(rest, &joint);
    assert(joint.token == "Arm" && joint.parent == 4);
    assert(SameOrientation(joint.restRotation, turn));
    assert(NearlyEqual(joint.restTranslation, pxr::GfVec3f(0.1f, 0.2f, 0.3f)));
    assert(NearlyEqual(joint.restScale, pxr::GfVec3f(2.0f, 3.0f, 0.5f)));

    openstrata::motion::SkeletonJoint unscaled;
    openstrata::motion::DecomposeRestTransform(pxr::GfMatrix4d(1.0), &unscaled);
    assert(unscaled.restScale == pxr::GfVec3f(1.0f));

    openstrata::motion::SkeletonJoint rescaled = joint;
    rescaled.restScale = pxr::GfVec3f(1.0f);
    assert(rescaled != joint && "a rest scale is part of a joint's identity");
}

openstrata::motion::SkeletonDescriptor
DesignAvatar()
{
    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::SkeletonJoint root;
    root.token = "Root";
    skeleton.AddJoint(root);

    openstrata::motion::SkeletonJoint pelvis;
    pelvis.token = "Root/Pelvis";
    pelvis.restTranslation = pxr::GfVec3f(0.0f, 1.0f, 0.0f);
    skeleton.AddJoint(pelvis);

    openstrata::motion::SkeletonJoint spine;
    spine.token = "Root/Pelvis/SpineA";
    spine.restTranslation = pxr::GfVec3f(0.0f, 0.5f, 0.0f);
    skeleton.AddJoint(spine);

    openstrata::motion::SkeletonJoint chest;
    chest.token = "Root/Pelvis/SpineA/ChestA";
    chest.restTranslation = pxr::GfVec3f(0.0f, 0.5f, 0.0f);
    skeleton.AddJoint(chest);

    skeleton.ResolveParentsFromTokens();
    return skeleton;
}

openstrata::motion::RetargetMap
DesignMap(const openstrata::motion::SkeletonDescriptor& skeleton)
{
    openstrata::motion::RetargetMap map;
    assert(map.SetJointToken(openstrata::motion::HumanJoint::Hips, "Root/Pelvis", skeleton));
    assert(map.SetJointToken(openstrata::motion::HumanJoint::Spine, "Root/Pelvis/SpineA", skeleton));
    assert(map.SetJointToken(openstrata::motion::HumanJoint::Chest, "Root/Pelvis/SpineA/ChestA", skeleton));
    return map;
}

// A caller's required-bone set: the seventeen bones a VRM 1.0 avatar must
// define, in the order usd-vrm-plugins supplies them. The library has no set of
// its own (RETARGETING_POLICY.md §4); these tests arrived with this one, and
// every count they assert was measured against it.
const std::vector<openstrata::motion::HumanJoint>&
DesignRequiredBones()
{
    using J = openstrata::motion::HumanJoint;
    static const std::vector<J> required = {
        J::Hips,          J::Spine,         J::Chest,        J::Neck,          J::Head,
        J::LeftUpperLeg,  J::LeftLowerLeg,  J::LeftFoot,     J::RightUpperLeg, J::RightLowerLeg,
        J::RightFoot,     J::LeftUpperArm,  J::LeftLowerArm, J::LeftHand,      J::RightUpperArm,
        J::RightLowerArm, J::RightHand,
    };
    return required;
}

openstrata::motion::RetargetOptions
DesignRequiredOptions()
{
    openstrata::motion::RetargetOptions options;
    options.requiredBones = DesignRequiredBones();
    return options;
}

// The clip's rest pose: hips at 1.0 m, matching canonical_walk.usda.
openstrata::motion::SourceRestPose
DesignSourceRest()
{
    openstrata::motion::SourceRestPose rest;
    rest.localTranslations[static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips)] =
        pxr::GfVec3f(0.0f, 1.0f, 0.0f);
    rest.SetParent(openstrata::motion::HumanJoint::Spine, openstrata::motion::HumanJoint::Hips);
    rest.SetParent(openstrata::motion::HumanJoint::Chest, openstrata::motion::HumanJoint::Spine);
    return rest;
}

// ---------------------------------------------------------------------------
// The partial skeleton policy (MOTION_CONTRACT.md, "Partial skeleton policy")
// ---------------------------------------------------------------------------

// Case 4: a rig that binds every bone VRM 1.0 requires and none of the optional
// ones -- no eyes, jaw, toes, shoulders, upperChest or fingers -- is a complete
// rig, and says nothing. What an optional bone does cost is reported only when
// a clip drives it, and then as a driven bone, not as a missing one.
void
TestAMissingOptionalBoneIsNotAMissingBone()
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    openstrata::motion::SkeletonDescriptor skeleton;
    for (const openstrata::motion::HumanJoint bone : DesignRequiredBones())
    {
        openstrata::motion::SkeletonJoint joint;
        joint.token = std::string(openstrata::motion::HumanJointName(bone));
        skeleton.AddJoint(joint);
    }
    skeleton.ResolveParentsFromTokens();
    openstrata::motion::RetargetMap map;
    for (const openstrata::motion::HumanJoint bone : DesignRequiredBones())
    {
        assert(map.SetJointToken(bone, std::string(openstrata::motion::HumanJointName(bone)), skeleton));
    }

    const openstrata::motion::RetargetDiagnostics rig =
        openstrata::motion::DiagnoseRig(skeleton, map, DesignRequiredOptions());
    assert(rig.reported.empty() && "a rig with every required bone reported something");

    openstrata::motion::MotionPose pose;
    for (const openstrata::motion::HumanJoint bone :
         {openstrata::motion::HumanJoint::Hips, openstrata::motion::HumanJoint::LeftEye, openstrata::motion::HumanJoint::Jaw,
          openstrata::motion::HumanJoint::LeftIndexProximal})
    {
        pose.localRotations[static_cast<std::size_t>(bone)] = Rotation(kAxisX, 10.0f);
        pose.validRotations.set(static_cast<std::size_t>(bone));
    }
    const openstrata::motion::PoseRetargeter retargeter(
        skeleton, map, openstrata::motion::SourceRestPose(), DesignRequiredOptions());
    openstrata::motion::RetargetDiagnostics diagnostics;
    const openstrata::motion::RetargetedPose result = retargeter.Retarget(pose, &diagnostics);
    assert(diagnostics.Subjects(Code::MissingRequiredBone).empty());
    assert((diagnostics.Subjects(Code::UnboundDrivenBone) ==
            std::vector<std::string>{"leftEye", "jaw", "leftIndexProximal"}));
    assert(SameOrientation(result.rotations[0], Rotation(kAxisX, 10.0f)));
}

// Cases 6 and 7: the clip and the rig disagree about the chain. The clip puts
// upperChest between chest and neck; the rig has no upperChest and puts a
// non-humanoid `Collar`, rested at a turn, there instead. Each bound bone keeps
// its motion *relative to its own parent* -- the rest correction reads each
// side's accumulated parent rest, the collar's turn included -- and the
// unbound bone's motion reaches nothing: it is not folded into its child.
void
TestAHierarchyMismatchCarriesEachBoneRelativeToItsOwnParent()
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    const pxr::GfQuatf collarRest = Rotation(kAxisZ, 30.0f);
    const pxr::GfQuatf neckRest = Rotation(kAxisX, 10.0f);

    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::SkeletonJoint chest;
    chest.token = "Chest";
    skeleton.AddJoint(chest);
    openstrata::motion::SkeletonJoint collar;
    collar.token = "Chest/Collar";
    collar.restRotation = collarRest;
    skeleton.AddJoint(collar);
    openstrata::motion::SkeletonJoint neck;
    neck.token = "Chest/Collar/Neck";
    neck.restRotation = neckRest;
    skeleton.AddJoint(neck);
    skeleton.ResolveParentsFromTokens();

    openstrata::motion::RetargetMap map;
    assert(map.SetJointToken(openstrata::motion::HumanJoint::Chest, "Chest", skeleton));
    assert(map.SetJointToken(openstrata::motion::HumanJoint::Neck, "Chest/Collar/Neck", skeleton));

    openstrata::motion::SourceRestPose sourceRest;
    sourceRest.SetParent(openstrata::motion::HumanJoint::UpperChest, openstrata::motion::HumanJoint::Chest);
    sourceRest.SetParent(openstrata::motion::HumanJoint::Neck, openstrata::motion::HumanJoint::UpperChest);

    const pxr::GfQuatf a = Rotation(kAxisY, 20.0f);
    const pxr::GfQuatf b = Rotation(kAxisX, 35.0f);
    const pxr::GfQuatf c = Rotation(kAxisZ, -25.0f);
    openstrata::motion::MotionPose pose;
    for (const auto& [bone, rotation] :
         {std::pair{openstrata::motion::HumanJoint::Chest, a}, std::pair{openstrata::motion::HumanJoint::UpperChest, b},
          std::pair{openstrata::motion::HumanJoint::Neck, c}})
    {
        pose.localRotations[static_cast<std::size_t>(bone)] = rotation;
        pose.validRotations.set(static_cast<std::size_t>(bone));
    }

    openstrata::motion::RetargetOptions options;
    options.rootMotion.mode = openstrata::motion::RootMotionMode::Ignore;
    const openstrata::motion::PoseRetargeter retargeter(skeleton, map, sourceRest, options);
    openstrata::motion::RetargetDiagnostics diagnostics;
    const openstrata::motion::RetargetedPose result = retargeter.Retarget(pose, &diagnostics);

    assert(
        (diagnostics.Subjects(Code::UnboundDrivenBone) == std::vector<std::string>{"upperChest"}));
    // The non-humanoid joint between them stays at its rest.
    assert(SameOrientation(result.rotations[1], collarRest));

    // Relative to its own parent, the neck moves as the clip's neck does: the
    // world delta over the parent's accumulated rest is `c` on both sides.
    const pxr::GfQuatf parentWorldRest = skeleton.GetWorldRestRotation(1);
    assert(SameOrientation(
        parentWorldRest * result.rotations[2] * (parentWorldRest * neckRest).GetInverse(), c));

    // In the world, the neck is where chest and neck put it -- `a * c` -- and
    // not where the clip's upperChest also turned it.
    const pxr::GfQuatf worldDelta = result.rotations[0] * result.rotations[1] *
                                    result.rotations[2] * (collarRest * neckRest).GetInverse();
    assert(SameOrientation(worldDelta, a * c));
    assert(!SameOrientation(worldDelta, a * b * c));
}

// Case 5: two bones on one joint. The retarget writes bones in vocabulary
// order, so the later bone a sample drives is the one the joint keeps, and the
// rig's report names the joint. (`execVrm`'s humanoid map refuses such a map
// instead: parity table row 3.)
void
TestADuplicateMappingKeepsTheLaterDrivenBone()
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    openstrata::motion::RetargetMap map = DesignMap(skeleton);
    assert(map.SetJointToken(openstrata::motion::HumanJoint::UpperChest, "Root/Pelvis/SpineA/ChestA", skeleton));

    openstrata::motion::MotionPose pose;
    pose.localRotations[static_cast<std::size_t>(openstrata::motion::HumanJoint::Chest)] =
        Rotation(kAxisX, 15.0f);
    pose.validRotations.set(static_cast<std::size_t>(openstrata::motion::HumanJoint::Chest));
    pose.localRotations[static_cast<std::size_t>(openstrata::motion::HumanJoint::UpperChest)] =
        Rotation(kAxisZ, 40.0f);
    pose.validRotations.set(static_cast<std::size_t>(openstrata::motion::HumanJoint::UpperChest));

    openstrata::motion::RetargetOptions options;
    options.rootMotion.mode = openstrata::motion::RootMotionMode::Ignore;
    const openstrata::motion::PoseRetargeter retargeter(skeleton, map, openstrata::motion::SourceRestPose(),
                                                 options);
    const openstrata::motion::RetargetedPose both = retargeter.Retarget(pose);
    assert(SameOrientation(both.rotations[3], Rotation(kAxisZ, 40.0f)));

    // Driven by the earlier bone alone, the joint follows it.
    pose.validRotations.reset(static_cast<std::size_t>(openstrata::motion::HumanJoint::UpperChest));
    const openstrata::motion::RetargetedPose chestOnly = retargeter.Retarget(pose);
    assert(SameOrientation(chestOnly.rotations[3], Rotation(kAxisX, 15.0f)));

    assert((openstrata::motion::DiagnoseRig(skeleton, map, options).Subjects(Code::DuplicateTarget) ==
            std::vector<std::string>{"Root/Pelvis/SpineA/ChestA"}));
}

void
TestSkeletonParentsComeFromJointPaths()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    const std::vector<openstrata::motion::SkeletonJoint>& joints = skeleton.GetJoints();
    assert(joints[0].parent == openstrata::motion::SkeletonDescriptor::kNoParent);
    assert(joints[1].parent == 0);
    assert(joints[2].parent == 1);
    assert(joints[3].parent == 2);
    assert(skeleton.IsTopologicallyOrdered());

    // Lookup is on the full joint path, not the leaf name.
    assert(skeleton.FindJoint("Root/Pelvis") == 1);
    assert(skeleton.FindJoint("Pelvis") == openstrata::motion::SkeletonDescriptor::kNoParent);

    // A joint whose parent path is absent is a root, not a dangling index.
    openstrata::motion::SkeletonDescriptor orphaned;
    openstrata::motion::SkeletonJoint stray;
    stray.token = "Missing/Child";
    orphaned.AddJoint(stray);
    orphaned.ResolveParentsFromTokens();
    assert(orphaned.GetJoints()[0].parent == openstrata::motion::SkeletonDescriptor::kNoParent);
}

void
TestRetargetMapReportsGapsAndCollisions()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    openstrata::motion::RetargetMap map = DesignMap(skeleton);

    assert(map.GetMappedCount() == 3);
    assert(map.IsMapped(openstrata::motion::HumanJoint::Hips));
    assert(!map.IsMapped(openstrata::motion::HumanJoint::Head));
    assert(map.GetJointIndex(openstrata::motion::HumanJoint::Head) == openstrata::motion::RetargetMap::kUnmapped);

    // An unknown token leaves the bone unmapped instead of guessing.
    assert(!map.SetJointToken(openstrata::motion::HumanJoint::Head, "NoSuchJoint", skeleton));
    assert(!map.IsMapped(openstrata::motion::HumanJoint::Head));

    const std::vector<openstrata::motion::HumanJoint> missing = map.FindMissingRequiredBones(DesignRequiredBones());
    assert(!missing.empty());
    assert(std::find(missing.begin(), missing.end(), openstrata::motion::HumanJoint::Head) != missing.end());
    assert(std::find(missing.begin(), missing.end(), openstrata::motion::HumanJoint::Hips) == missing.end());

    assert(map.FindDuplicateJointIndices().empty());
    assert(map.SetJointToken(openstrata::motion::HumanJoint::UpperChest, "Root/Pelvis/SpineA/ChestA", skeleton));
    const std::vector<int> duplicates = map.FindDuplicateJointIndices();
    assert(duplicates.size() == 1 && duplicates[0] == 3);
}

// The rig values are compared exactly, because the caller that asked for the
// comparison -- OpenExec's type registry, for `execVrm` -- wants "is this the
// same recorded value?", not "is this the same rig?".
void
TestRigValuesCompareExactly()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    assert(skeleton == DesignAvatar());
    assert(!(skeleton != DesignAvatar()));

    // Every field of a joint is part of the value.
    std::vector<openstrata::motion::SkeletonJoint> joints = skeleton.GetJoints();
    joints[2].restTranslation[1] += 1e-6f;
    assert(openstrata::motion::SkeletonDescriptor(joints) != skeleton);
    joints = skeleton.GetJoints();
    joints[2].parent = openstrata::motion::SkeletonDescriptor::kNoParent;
    assert(openstrata::motion::SkeletonDescriptor(joints) != skeleton);
    joints = skeleton.GetJoints();
    joints[2].token = "Root/Pelvis/SpineB";
    assert(openstrata::motion::SkeletonDescriptor(joints) != skeleton);

    // A rest rotation and its negation rest identically and are different
    // values -- the conservative answer, and MotionPose's.
    joints = skeleton.GetJoints();
    joints[1].restRotation = pxr::GfQuatf(-1.0f, pxr::GfVec3f(0.0f));
    assert(SameOrientation(joints[1].restRotation, skeleton.GetJoints()[1].restRotation));
    assert(openstrata::motion::SkeletonDescriptor(joints) != skeleton);

    // The joint ORDER is part of the value: it is what a map's indices count
    // into.
    joints = skeleton.GetJoints();
    std::swap(joints[2], joints[3]);
    assert(openstrata::motion::SkeletonDescriptor(joints) != skeleton);

    const openstrata::motion::RetargetMap map = DesignMap(skeleton);
    assert(map == DesignMap(skeleton));
    assert(map != openstrata::motion::RetargetMap());

    openstrata::motion::RetargetMap moved = DesignMap(skeleton);
    assert(moved.SetJointToken(openstrata::motion::HumanJoint::Chest, "Root/Pelvis/SpineA", skeleton));
    assert(moved != map);

    // A rejected binding of a bone that was never mapped leaves the map as it
    // was, and so equal to it.
    openstrata::motion::RetargetMap rejected = DesignMap(skeleton);
    assert(!rejected.SetJointToken(openstrata::motion::HumanJoint::Head, "NoSuchJoint", skeleton));
    assert(rejected == map);

    // The correction, for `vrm.computeRestPoseCorrection`: a rig whose hips
    // rest is turned, so the correction has a slot that is not identity.
    std::vector<openstrata::motion::SkeletonJoint> turnedJoints = skeleton.GetJoints();
    turnedJoints[1].restRotation = Rotation(kAxisY, 30.0f);
    const openstrata::motion::SkeletonDescriptor turned(turnedJoints);
    const openstrata::motion::RestPoseCorrection correction =
        openstrata::motion::ComputeRestPoseCorrection(DesignSourceRest(), turned, map);
    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);
    assert(!correction.identity[hips]);
    assert(correction == openstrata::motion::ComputeRestPoseCorrection(DesignSourceRest(), turned, map));
    assert(correction != openstrata::motion::RestPoseCorrection());

    // Each of the three halves is part of the value.
    openstrata::motion::RestPoseCorrection changed = correction;
    changed.pre[hips] = Rotation(kAxisY, 31.0f);
    assert(changed != correction);
    changed = correction;
    changed.post[hips] = Rotation(kAxisY, 31.0f);
    assert(changed != correction);
    // The flag alone: `Apply` reads it first, so the same pre and post answer
    // differently once it is set.
    changed = correction;
    changed.identity[hips] = true;
    assert(changed != correction);

    // A negated half applies identically and is a different value.
    changed = correction;
    changed.pre[hips] =
        pxr::GfQuatf(-correction.pre[hips].GetReal(), -correction.pre[hips].GetImaginary());
    const pxr::GfQuatf sample = Rotation(kAxisX, 20.0f);
    assert(SameOrientation(changed.Apply(openstrata::motion::HumanJoint::Hips, sample),
                           correction.Apply(openstrata::motion::HumanJoint::Hips, sample)));
    assert(changed != correction);

    // The retargeted pose, for `vrm.humanoidRetarget`: one sample expanded
    // onto the turned rig, so its arrays are not all rest.
    openstrata::motion::MotionPose source;
    source.timestamp = 0.5;
    source.localRotations[hips] = Rotation(kAxisX, 20.0f);
    source.validRotations.set(hips);
    source.root.worldPosition = pxr::GfVec3f(0.1f, 1.0f, 0.0f);
    source.root.hasPosition = true;
    const openstrata::motion::PoseRetargeter retargeter(turned, map, DesignSourceRest());
    const openstrata::motion::RetargetedPose pose = retargeter.Retarget(source);
    assert(pose == retargeter.Retarget(source));
    assert(!(pose != retargeter.Retarget(source)));
    assert(pose != openstrata::motion::RetargetedPose());

    // Each of the three fields is part of the value.
    openstrata::motion::RetargetedPose other = pose;
    other.timestamp = 0.25;
    assert(other != pose);
    other = pose;
    other.translations[1][0] += 1e-6f;
    assert(other != pose);
    other = pose;
    other.rotations[1] = Rotation(kAxisX, 21.0f);
    assert(other != pose);

    // A negated rotation poses the joint identically and is a different value.
    other = pose;
    other.rotations[1] =
        pxr::GfQuatf(-pose.rotations[1].GetReal(), -pose.rotations[1].GetImaginary());
    assert(SameOrientation(other.rotations[1], pose.rotations[1]));
    assert(other != pose);

    // The slot ORDER is part of the value: it is the rig's joint order.
    other = pose;
    std::swap(other.rotations[1], other.rotations[2]);
    assert(other != pose);

    // The same sample in an animation's shape, for
    // `vrm.computeJointLocalTransforms`: the pose's arrays, the rig's tokens,
    // and one identity scale per joint.
    openstrata::motion::JointLocalTransforms baked;
    baked.timestamp = pose.timestamp;
    for (const openstrata::motion::SkeletonJoint& joint : turned.GetJoints())
    {
        baked.joints.push_back(joint.token);
    }
    baked.translations = pose.translations;
    baked.rotations = pose.rotations;
    baked.scales.assign(turned.GetSize(), pxr::GfVec3h(1.0f));
    const openstrata::motion::JointLocalTransforms same = baked;
    assert(baked == same);
    assert(!(baked != same));
    assert(baked != openstrata::motion::JointLocalTransforms());

    // Each of the five fields is part of the value -- the joints and the
    // scales included, although neither moves from sample to sample of one
    // bake.
    openstrata::motion::JointLocalTransforms changedBaked = baked;
    changedBaked.timestamp = 0.25;
    assert(changedBaked != baked);
    changedBaked = baked;
    changedBaked.joints[2] = "Root/Pelvis/SpineB";
    assert(changedBaked != baked);
    changedBaked = baked;
    changedBaked.translations[1][0] += 1e-6f;
    assert(changedBaked != baked);
    changedBaked = baked;
    changedBaked.rotations[1] =
        pxr::GfQuatf(-pose.rotations[1].GetReal(), -pose.rotations[1].GetImaginary());
    assert(changedBaked != baked);
    changedBaked = baked;
    changedBaked.scales[1] = pxr::GfVec3h(2.0f);
    assert(changedBaked != baked);

    // An animation that names its joints in another order is another value,
    // even carrying the same arrays: the order is what `joints` states.
    changedBaked = baked;
    std::swap(changedBaked.joints[1], changedBaked.joints[2]);
    assert(changedBaked != baked);
}

// A rejected rebinding unmaps the bone, whether the index or the token is what
// the skeleton cannot honour -- the two setters answer one question one way,
// and a `false` never leaves an earlier binding standing behind it.
void
TestARejectedRebindingUnmapsTheBone()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();

    openstrata::motion::RetargetMap byToken = DesignMap(skeleton);
    assert(byToken.IsMapped(openstrata::motion::HumanJoint::Spine));
    assert(!byToken.SetJointToken(openstrata::motion::HumanJoint::Spine, "NoSuchJoint", skeleton));
    assert(!byToken.IsMapped(openstrata::motion::HumanJoint::Spine) &&
           "a failed token lookup left the earlier binding standing");
    assert(byToken.GetJointIndex(openstrata::motion::HumanJoint::Spine) == openstrata::motion::RetargetMap::kUnmapped);
    assert(byToken.GetMappedCount() == 2);

    openstrata::motion::RetargetMap byIndex = DesignMap(skeleton);
    assert(!byIndex.SetJointIndex(openstrata::motion::HumanJoint::Spine, 99, skeleton.GetSize()));
    assert(!byIndex.IsMapped(openstrata::motion::HumanJoint::Spine));

    // Both routes land on the same map.
    assert(byToken == byIndex);
}

void
TestIdentityRestPosesPassRotationsThrough()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    const openstrata::motion::RetargetMap map = DesignMap(skeleton);
    const openstrata::motion::RestPoseCorrection correction =
        openstrata::motion::ComputeRestPoseCorrection(DesignSourceRest(), skeleton, map);

    const pxr::GfQuatf sample = Rotation(kAxisY, 90.0f);
    assert(SameOrientation(correction.Apply(openstrata::motion::HumanJoint::Hips, sample), sample));
    assert(correction.identity[static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips)]);
}

// The correction's contract is that the bone's world rotation *away from its
// own rest* survives the change of rig. Verify it directly rather than
// trusting the closed form.
void
TestRestPoseCorrectionPreservesTheWorldDelta()
{
    const pxr::GfQuatf sourceParentRest = Rotation(kAxisX, 20.0f);
    const pxr::GfQuatf sourceRest = Rotation(kAxisZ, -35.0f);
    const pxr::GfQuatf targetParentRest = Rotation(kAxisY, 50.0f);
    const pxr::GfQuatf targetRest = Rotation(kAxisX, 15.0f);

    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::SkeletonJoint parent;
    parent.token = "Hips";
    parent.restRotation = targetParentRest;
    skeleton.AddJoint(parent);
    openstrata::motion::SkeletonJoint child;
    child.token = "Hips/Spine";
    child.restRotation = targetRest;
    skeleton.AddJoint(child);
    skeleton.ResolveParentsFromTokens();

    openstrata::motion::SourceRestPose sourceRestPose;
    sourceRestPose.localRotations[static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips)] =
        sourceParentRest;
    sourceRestPose.localRotations[static_cast<std::size_t>(openstrata::motion::HumanJoint::Spine)] = sourceRest;
    sourceRestPose.SetParent(openstrata::motion::HumanJoint::Spine, openstrata::motion::HumanJoint::Hips);

    openstrata::motion::RetargetMap map;
    map.SetJointToken(openstrata::motion::HumanJoint::Hips, "Hips", skeleton);
    map.SetJointToken(openstrata::motion::HumanJoint::Spine, "Hips/Spine", skeleton);

    const openstrata::motion::RestPoseCorrection correction =
        openstrata::motion::ComputeRestPoseCorrection(sourceRestPose, skeleton, map);
    assert(!correction.identity[static_cast<std::size_t>(openstrata::motion::HumanJoint::Spine)]);

    const pxr::GfQuatf animated = Rotation(kAxisY, 42.0f) * sourceRest;
    const pxr::GfQuatf retargeted = correction.Apply(openstrata::motion::HumanJoint::Spine, animated);

    // World delta = worldAnimated * worldRest^-1, with world = parent * local
    // (OpenUSD composition: `a * b` applies `b` first).
    const pxr::GfQuatf sourceDelta =
        (sourceParentRest * animated) * (sourceParentRest * sourceRest).GetInverse();
    const pxr::GfQuatf targetDelta =
        (targetParentRest * retargeted) * (targetParentRest * targetRest).GetInverse();
    assert(SameOrientation(sourceDelta, targetDelta));

    // A sample sitting at the source rest must land exactly on the target rest.
    assert(SameOrientation(correction.Apply(openstrata::motion::HumanJoint::Spine, sourceRest), targetRest));
}

// The same invariant one level deeper. A grandparent's rest rotation reaches
// the bone only through the accumulated chain, so a correction built from each
// parent's own local rotation passes the two-level test above and fails here.
void
TestRestPoseCorrectionAccountsForTheWholeAncestorChain()
{
    const pxr::GfQuatf targetHipsRest = Rotation(kAxisY, 25.0f);
    const pxr::GfQuatf targetSpineRest = Rotation(kAxisX, -15.0f);
    const pxr::GfQuatf targetChestRest = Rotation(kAxisZ, 40.0f);
    const pxr::GfQuatf sourceHipsRest = Rotation(kAxisZ, -10.0f);
    const pxr::GfQuatf sourceSpineRest = Rotation(kAxisY, 30.0f);
    const pxr::GfQuatf sourceChestRest = Rotation(kAxisX, 55.0f);

    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::SkeletonJoint hips;
    hips.token = "Hips";
    hips.restRotation = targetHipsRest;
    skeleton.AddJoint(hips);
    openstrata::motion::SkeletonJoint spine;
    spine.token = "Hips/Spine";
    spine.restRotation = targetSpineRest;
    skeleton.AddJoint(spine);
    openstrata::motion::SkeletonJoint chest;
    chest.token = "Hips/Spine/Chest";
    chest.restRotation = targetChestRest;
    skeleton.AddJoint(chest);
    skeleton.ResolveParentsFromTokens();

    openstrata::motion::SourceRestPose sourceRest;
    sourceRest.localRotations[static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips)] = sourceHipsRest;
    sourceRest.localRotations[static_cast<std::size_t>(openstrata::motion::HumanJoint::Spine)] = sourceSpineRest;
    sourceRest.localRotations[static_cast<std::size_t>(openstrata::motion::HumanJoint::Chest)] = sourceChestRest;
    sourceRest.SetParent(openstrata::motion::HumanJoint::Spine, openstrata::motion::HumanJoint::Hips);
    sourceRest.SetParent(openstrata::motion::HumanJoint::Chest, openstrata::motion::HumanJoint::Spine);

    // Both accumulators compose root-first.
    assert(SameOrientation(skeleton.GetWorldRestRotation(2),
                           targetHipsRest * targetSpineRest * targetChestRest));
    assert(SameOrientation(sourceRest.GetWorldRestRotation(openstrata::motion::HumanJoint::Chest),
                           sourceHipsRest * sourceSpineRest * sourceChestRest));
    // A root joint's absent parent contributes identity, not a dangling index.
    assert(SameOrientation(skeleton.GetWorldRestRotation(openstrata::motion::SkeletonDescriptor::kNoParent),
                           pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f))));

    openstrata::motion::RetargetMap map;
    map.SetJointToken(openstrata::motion::HumanJoint::Hips, "Hips", skeleton);
    map.SetJointToken(openstrata::motion::HumanJoint::Spine, "Hips/Spine", skeleton);
    map.SetJointToken(openstrata::motion::HumanJoint::Chest, "Hips/Spine/Chest", skeleton);

    const openstrata::motion::RestPoseCorrection correction =
        openstrata::motion::ComputeRestPoseCorrection(sourceRest, skeleton, map);

    const pxr::GfQuatf animated = Rotation(kAxisY, 42.0f) * sourceChestRest;
    const pxr::GfQuatf retargeted = correction.Apply(openstrata::motion::HumanJoint::Chest, animated);

    const pxr::GfQuatf sourceParentWorld = sourceHipsRest * sourceSpineRest;
    const pxr::GfQuatf targetParentWorld = targetHipsRest * targetSpineRest;
    const pxr::GfQuatf sourceDelta =
        (sourceParentWorld * animated) * (sourceParentWorld * sourceChestRest).GetInverse();
    const pxr::GfQuatf targetDelta =
        (targetParentWorld * retargeted) * (targetParentWorld * targetChestRest).GetInverse();
    assert(SameOrientation(sourceDelta, targetDelta));

    // And the rest pose itself still maps onto the target's rest pose.
    assert(SameOrientation(correction.Apply(openstrata::motion::HumanJoint::Chest, sourceChestRest),
                           targetChestRest));
}

void
TestTargetReferenceRestIsSeparateFromUsdSkelRest()
{
    using J = openstrata::motion::HumanJoint;
    const pxr::GfQuatf armAim = Rotation(kAxisZ, 40.0f);
    const pxr::GfQuatf shoulderAim = Rotation(kAxisY, 15.0f);
    const pxr::GfQuatf identity(1.0f, pxr::GfVec3f(0.0f));

    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::SkeletonJoint shoulder;
    shoulder.token = "Shoulder";
    skeleton.AddJoint(shoulder);
    openstrata::motion::SkeletonJoint arm;
    arm.token = "Shoulder/Arm";
    skeleton.AddJoint(arm);
    skeleton.ResolveParentsFromTokens();

    openstrata::motion::RetargetMap map;
    assert(map.SetJointIndex(J::LeftShoulder, 0, skeleton.GetSize()));
    assert(map.SetJointIndex(J::LeftUpperArm, 1, skeleton.GetSize()));

    openstrata::motion::TargetRestPose targetRest;
    targetRest.localRotations.resize(skeleton.GetSize());
    targetRest.localRotations[0] = shoulderAim;
    targetRest.localRotations[1] = armAim;
    assert(SameOrientation(targetRest.GetWorldRestRotation(skeleton, 1), shoulderAim * armAim));
    assert(SameOrientation(skeleton.GetWorldRestRotation(1), identity));

    openstrata::motion::SourceRestPose sourceRest;
    sourceRest.SetParent(J::LeftUpperArm, J::LeftShoulder);
    const auto correction = openstrata::motion::ComputeRestPoseCorrection(
        sourceRest, skeleton, map, targetRest);
    assert(SameOrientation(correction.Apply(J::LeftShoulder, identity), shoulderAim));
    assert(SameOrientation(correction.Apply(J::LeftUpperArm, identity), armAim));
    const pxr::GfQuatf animated = Rotation(kAxisX, 25.0f);
    const pxr::GfQuatf corrected = correction.Apply(J::LeftUpperArm, animated);
    const pxr::GfQuatf targetDelta =
        (shoulderAim * corrected) * (shoulderAim * armAim).GetInverse();
    assert(SameOrientation(targetDelta, animated));

    // A source stating the same reference rest as the target needs no offset.
    sourceRest.localRotations[static_cast<std::size_t>(J::LeftShoulder)] = shoulderAim;
    sourceRest.localRotations[static_cast<std::size_t>(J::LeftUpperArm)] = armAim;
    const auto sameRest = openstrata::motion::ComputeRestPoseCorrection(
        sourceRest, skeleton, map, targetRest);
    assert(SameOrientation(sameRest.Apply(J::LeftUpperArm, armAim), armAim));

    openstrata::motion::RetargetOptions options;
    options.targetRest = targetRest;
    const openstrata::motion::PoseRetargeter retargeter(skeleton, map,
                                                       openstrata::motion::SourceRestPose(), options);
    openstrata::motion::MotionPose pose;
    pose.validRotations.set(static_cast<std::size_t>(J::LeftUpperArm));
    const auto driven = retargeter.Retarget(pose);
    assert(SameOrientation(driven.rotations[1], armAim));
    assert(SameOrientation(driven.rotations[0], identity));

    pose.validRotations.reset();
    const auto undriven = retargeter.Retarget(pose);
    assert(SameOrientation(undriven.rotations[0], identity));
    assert(SameOrientation(undriven.rotations[1], identity));

    // An empty reference rest preserves the existing correction exactly.
    assert(openstrata::motion::ComputeRestPoseCorrection(
               sourceRest, skeleton, map) ==
           openstrata::motion::ComputeRestPoseCorrection(
               sourceRest, skeleton, map, openstrata::motion::TargetRestPose()));
}

void
TestRootMotionModes()
{
    const pxr::GfVec3f sourceRest(0.0f, 1.0f, 0.0f);
    const pxr::GfVec3f sourceNow(0.0f, 1.2f, 0.5f);
    const pxr::GfVec3f targetRest(0.0f, 1.6f, 0.0f);

    openstrata::motion::RootMotionOptions options;
    options.mode = openstrata::motion::RootMotionMode::Ignore;
    assert(
        NearlyEqual(openstrata::motion::ResolveRootTranslation(options, sourceNow, sourceRest, targetRest),
                    targetRest));

    // The delta carries, not the absolute height: a 1.0 m rig drives a 1.6 m
    // one without the avatar snapping to the source's hip height.
    options.mode = openstrata::motion::RootMotionMode::Hips;
    assert(
        NearlyEqual(openstrata::motion::ResolveRootTranslation(options, sourceNow, sourceRest, targetRest),
                    pxr::GfVec3f(0.0f, 1.8f, 0.5f)));

    options.translationScale = 2.0f;
    assert(
        NearlyEqual(openstrata::motion::ResolveRootTranslation(options, sourceNow, sourceRest, targetRest),
                    pxr::GfVec3f(0.0f, 2.0f, 1.0f)));

    options.translationScale = 1.0f;
    options.preserveTargetHeight = true;
    assert(
        NearlyEqual(openstrata::motion::ResolveRootTranslation(options, sourceNow, sourceRest, targetRest),
                    pxr::GfVec3f(0.0f, 1.6f, 0.5f)));
}

openstrata::motion::MotionClip
DesignClip()
{
    openstrata::motion::MotionClip animation;
    animation.startTime = 0.0;
    animation.endTime = 1.0;
    animation.nominalFrameRate = 30.0;

    const auto hips = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);
    const auto spine = static_cast<std::size_t>(openstrata::motion::HumanJoint::Spine);
    const auto chest = static_cast<std::size_t>(openstrata::motion::HumanJoint::Chest);

    openstrata::motion::MotionPose first;
    first.timestamp = 0.0;
    first.validRotations.set(hips);
    first.validRotations.set(spine);
    first.validRotations.set(chest);
    first.root.worldPosition = pxr::GfVec3f(0.0f, 1.0f, 0.0f);
    first.root.hasPosition = true;
    animation.samples.push_back(first);

    openstrata::motion::MotionPose last;
    last.timestamp = 1.0;
    last.localRotations[hips] = Rotation(kAxisY, 90.0f);
    last.localRotations[chest] = Rotation(kAxisX, 90.0f);
    last.validRotations.set(hips);
    last.validRotations.set(spine);
    last.validRotations.set(chest);
    last.root.worldPosition = pxr::GfVec3f(0.0f, 1.0f, 0.5f);
    last.root.hasPosition = true;
    animation.samples.push_back(last);

    return animation;
}

// Reproduces docs/design/fixtures/motion/expected_retargeted.usda from
// canonical_walk.usda's values against avatar.usda's rig.
void
TestDesignTripletHandOff()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    const openstrata::motion::PoseRetargeter retargeter(skeleton, DesignMap(skeleton), DesignSourceRest(),
                                                        DesignRequiredOptions());

    openstrata::motion::RetargetDiagnostics diagnostics;
    const openstrata::motion::RetargetedAnimation result = retargeter.Retarget(DesignClip(), &diagnostics);

    assert(result.joints.size() == 4);
    assert(result.joints[0] == "Root");
    assert(result.joints[3] == "Root/Pelvis/SpineA/ChestA");
    assert(result.samples.size() == 2);

    const openstrata::motion::RetargetedPose& first = result.samples.front();
    for (const pxr::GfQuatf& rotation : first.rotations)
    {
        assert(SameOrientation(rotation, pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f))));
    }
    assert(NearlyEqual(first.translations[0], pxr::GfVec3f(0.0f)));
    assert(NearlyEqual(first.translations[1], pxr::GfVec3f(0.0f, 1.0f, 0.0f)));
    assert(NearlyEqual(first.translations[2], pxr::GfVec3f(0.0f, 0.5f, 0.0f)));
    assert(NearlyEqual(first.translations[3], pxr::GfVec3f(0.0f, 0.5f, 0.0f)));

    const openstrata::motion::RetargetedPose& last = result.samples.back();
    // Root is unmapped: it holds its rest pose while Pelvis takes the hips.
    assert(SameOrientation(last.rotations[0], pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f))));
    assert(SameOrientation(last.rotations[1], Rotation(kAxisY, 90.0f)));
    assert(SameOrientation(last.rotations[2], pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f))));
    assert(SameOrientation(last.rotations[3], Rotation(kAxisX, 90.0f)));
    assert(NearlyEqual(last.translations[0], pxr::GfVec3f(0.0f)));
    assert(NearlyEqual(last.translations[1], pxr::GfVec3f(0.0f, 1.0f, 0.5f)));
    assert(NearlyEqual(last.translations[2], pxr::GfVec3f(0.0f, 0.5f, 0.0f)));
    assert(NearlyEqual(last.translations[3], pxr::GfVec3f(0.0f, 0.5f, 0.0f)));

    // The rig maps three bones, so the other fourteen required bones are
    // reported, one each and in the order the caller's set states them -- and nothing the clip drives
    // is unbound.
    using Code = openstrata::motion::RetargetDiagnosticCode;
    const std::vector<std::string> missing = diagnostics.Subjects(Code::MissingRequiredBone);
    assert(missing.size() == 14);
    assert(missing.front() == "neck");
    assert(missing.back() == "rightHand");
    assert(diagnostics.Subjects(Code::UnboundDrivenBone).empty());
    assert(diagnostics.reported.size() == 14);
}

void
TestUnmappedJointsStayAtRestAndAreReported()
{
    openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    openstrata::motion::RetargetMap map;
    // Bind hips only: the clip also drives spine and chest.
    map.SetJointToken(openstrata::motion::HumanJoint::Hips, "Root/Pelvis", skeleton);

    const openstrata::motion::PoseRetargeter retargeter(skeleton, map, DesignSourceRest());
    openstrata::motion::RetargetDiagnostics diagnostics;
    const openstrata::motion::RetargetedAnimation result = retargeter.Retarget(DesignClip(), &diagnostics);

    const openstrata::motion::RetargetedPose& last = result.samples.back();
    // SpineA and ChestA keep their rest transforms rather than collapsing.
    assert(SameOrientation(last.rotations[2], pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f))));
    assert(SameOrientation(last.rotations[3], pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f))));
    assert(NearlyEqual(last.translations[3], pxr::GfVec3f(0.0f, 0.5f, 0.0f)));

    // Two bones driven on both samples, reported once each.
    const std::vector<std::string> unbound =
        diagnostics.Subjects(openstrata::motion::RetargetDiagnosticCode::UnboundDrivenBone);
    assert((unbound == std::vector<std::string>{"spine", "chest"}));
}

// WS-O2: the retarget answers at the clip's own times and never resamples. It
// used to take a rate and resample first, which is what tied it to the
// sampling library; a caller that wants a uniform timeline resamples before it
// retargets.
void
TestAClipIsRetargetedAtItsOwnSampleTimes()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    const openstrata::motion::PoseRetargeter retargeter(skeleton, DesignMap(skeleton),
                                                        DesignSourceRest());

    const openstrata::motion::MotionClip clip = DesignClip();
    const openstrata::motion::RetargetedAnimation result = retargeter.Retarget(clip);
    assert(result.samples.size() == clip.samples.size());
    assert(result.frameRate == clip.nominalFrameRate);
    for (std::size_t i = 0; i < clip.samples.size(); ++i)
    {
        assert(result.samples[i].timestamp == clip.samples[i].timestamp);
    }
    assert(result.startTime == 0.0 && result.endTime == 1.0);
    // The hips land where the last sample put them, with nothing in between.
    assert(NearlyEqual(result.samples[1].translations[1], pxr::GfVec3f(0.0f, 1.0f, 0.5f)));
}

void
TestRootJointModeMovesTheReceiver()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    openstrata::motion::RetargetOptions options;
    options.rootMotion.mode = openstrata::motion::RootMotionMode::RootJoint;
    options.rootMotion.rootJointIndex = 0;
    const openstrata::motion::PoseRetargeter retargeter(skeleton, DesignMap(skeleton), DesignSourceRest(),
                                                 options);

    const openstrata::motion::RetargetedAnimation result = retargeter.Retarget(DesignClip());
    const openstrata::motion::RetargetedPose& last = result.samples.back();
    // Root takes the delta; Pelvis stays at its rest translation.
    assert(NearlyEqual(last.translations[0], pxr::GfVec3f(0.0f, 0.0f, 0.5f)));
    assert(NearlyEqual(last.translations[1], pxr::GfVec3f(0.0f, 1.0f, 0.0f)));

    // An invalid root index degrades to "author nothing" and says so.
    options.rootMotion.rootJointIndex = -1;
    const openstrata::motion::PoseRetargeter degraded(skeleton, DesignMap(skeleton), DesignSourceRest(),
                                               options);
    openstrata::motion::RetargetDiagnostics diagnostics;
    const openstrata::motion::RetargetedAnimation fallback = degraded.Retarget(DesignClip(), &diagnostics);
    assert(NearlyEqual(fallback.samples.back().translations[1], pxr::GfVec3f(0.0f, 1.0f, 0.0f)));
    // Once, from the rig's report, although both samples dropped a root.
    assert(diagnostics.Subjects(openstrata::motion::RetargetDiagnosticCode::InvalidRootJoint) ==
           std::vector<std::string>{"-1"});
}

// ---------------------------------------------------------------------------
// Retarget diagnostics (the OpenExec plan's P1-1): a frozen code set, raised as
// values, so that two implementations of a retarget can be compared on what
// they reported as well as on what they computed.
// ---------------------------------------------------------------------------

// The table is the contract: every code has its own stable string under the
// retarget prefix, the strings round-trip, and the two halves of the set sit
// where the layer check expects them.
void
TestTheRetargetCodeTableIsClosedAndStable()
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    const std::vector<std::string> expected = {
        "MOTION_RETARGET_MISSING_REQUIRED_BONE", "MOTION_RETARGET_UNBOUND_DRIVEN_BONE",
        "MOTION_RETARGET_DUPLICATE_TARGET",      "MOTION_RETARGET_INVALID_HIERARCHY",
        "MOTION_RETARGET_INVALID_ROOT_JOINT",    "MOTION_RETARGET_NON_UNIT_SCALE",
        "MOTION_RETARGET_TIME_RANGE_DERIVED",    "MOTION_RETARGET_OUTPUT_COLLIDES_WITH_INPUT",
    };
    assert(expected.size() == openstrata::motion::RetargetDiagnosticCodeCount);
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        const auto code = static_cast<Code>(i);
        assert(openstrata::motion::RetargetDiagnosticCodeString(code) == expected[i]);
        assert(openstrata::motion::FindRetargetDiagnosticCode(expected[i]) == code);
        // The library raises exactly the leading five.
        assert(openstrata::motion::RetargetDiagnosticIsLibraryRaised(code) == (i < 5));
    }
    assert(openstrata::motion::RetargetDiagnosticCodeString(Code::Count).empty());
    assert(!openstrata::motion::FindRetargetDiagnosticCode("MOTION_BVH_PARSE_FAILED"));

    // Only a collision stops a retarget, and only a derived time range is
    // merely informative.
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        const auto code = static_cast<Code>(i);
        assert(openstrata::motion::RetargetDiagnosticIsRecoverable(code) ==
               (code != Code::OutputCollidesWithInput));
    }
    assert(openstrata::motion::RetargetDiagnosticDefaultSeverity(Code::OutputCollidesWithInput) ==
           openstrata::motion::RetargetDiagnosticSeverity::Error);
    assert(openstrata::motion::RetargetDiagnosticDefaultSeverity(Code::TimeRangeDerived) ==
           openstrata::motion::RetargetDiagnosticSeverity::Info);
    assert(openstrata::motion::RetargetDiagnosticDefaultSeverity(Code::UnboundDrivenBone) ==
           openstrata::motion::RetargetDiagnosticSeverity::Warning);
}

// A diagnostic built from a code takes the table's severity, and its line is
// fixed field by field.
void
TestARetargetDiagnosticFormatsOneStableLine()
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    const openstrata::motion::RetargetDiagnostic unbound =
        openstrata::motion::MakeRetargetDiagnostic(Code::UnboundDrivenBone, "upperChest", "no joint");
    assert(unbound.severity == openstrata::motion::RetargetDiagnosticSeverity::Warning);
    assert(unbound.recoverable);
    assert(openstrata::motion::FormatRetargetDiagnostic(unbound) ==
           "[MOTION_RETARGET_UNBOUND_DRIVEN_BONE] warning recoverable "
           "subject=upperChest: no joint");

    const openstrata::motion::RetargetDiagnostic collision =
        openstrata::motion::MakeRetargetDiagnostic(Code::OutputCollidesWithInput, "clip.usda");
    assert(openstrata::motion::FormatRetargetDiagnostic(collision) ==
           "[MOTION_RETARGET_OUTPUT_COLLIDES_WITH_INPUT] error "
           "subject=clip.usda");
}

// A code and a subject are reported once, the first report wins, and two lists
// compare entry by entry in order.
void
TestADiagnosticIsReportedOncePerCodeAndSubject()
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    openstrata::motion::RetargetDiagnostics diagnostics;
    assert(diagnostics.IsClean());
    assert(diagnostics.Report(
        openstrata::motion::MakeRetargetDiagnostic(Code::UnboundDrivenBone, "jaw", "a")));
    assert(!diagnostics.Report(
        openstrata::motion::MakeRetargetDiagnostic(Code::UnboundDrivenBone, "jaw", "b")));
    // The same subject under another code is another fact.
    assert(diagnostics.Report(
        openstrata::motion::MakeRetargetDiagnostic(Code::MissingRequiredBone, "jaw", "c")));
    assert(diagnostics.reported.size() == 2);
    assert(diagnostics.reported[0].detail == "a");
    assert(diagnostics.Has(Code::UnboundDrivenBone, "jaw"));
    assert(!diagnostics.Has(Code::UnboundDrivenBone, "neck"));

    openstrata::motion::RetargetDiagnostics copy;
    copy.Merge(diagnostics);
    copy.Merge(diagnostics);
    assert(copy == diagnostics);

    openstrata::motion::RetargetDiagnostics reversed;
    reversed.Report(diagnostics.reported[1]);
    reversed.Report(diagnostics.reported[0]);
    assert(reversed != diagnostics);
}

// The rig's own report, apart from any clip: a duplicate names the joint and
// both bones, a hierarchy defect names the first joint out of order, and a
// missing hips joint says what it costs only when root motion would land there.
void
TestTheRigIsDiagnosedBeforeAnyClip()
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    openstrata::motion::RetargetMap map = DesignMap(skeleton);
    assert(map.SetJointToken(openstrata::motion::HumanJoint::UpperChest, "Root/Pelvis/SpineA/ChestA", skeleton));

    const openstrata::motion::RetargetDiagnostics rig =
        openstrata::motion::DiagnoseRig(skeleton, map, DesignRequiredOptions());
    assert(rig.Subjects(Code::DuplicateTarget) ==
           std::vector<std::string>{"Root/Pelvis/SpineA/ChestA"});
    const std::string& duplicate = rig.reported[rig.reported.size() - 1].detail;
    assert(duplicate.find("'chest'") != std::string::npos);
    assert(duplicate.find("'upperChest'") != std::string::npos);
    assert(rig.Subjects(Code::InvalidHierarchy).empty());
    assert(rig.Subjects(Code::InvalidRootJoint).empty());

    // A child listed before its parent.
    std::vector<openstrata::motion::SkeletonJoint> joints = skeleton.GetJoints();
    std::swap(joints[1], joints[2]);
    joints[1].parent = 2;
    joints[2].parent = 0;
    const openstrata::motion::SkeletonDescriptor unordered(joints);
    const openstrata::motion::RetargetDiagnostics hierarchy =
        openstrata::motion::DiagnoseRig(unordered, openstrata::motion::RetargetMap());
    assert(hierarchy.Subjects(Code::InvalidHierarchy) ==
           std::vector<std::string>{"Root/Pelvis/SpineA"});

    // No hips: under 'hips' the root lands nowhere, and the detail says so --
    // with or without a required set, since the mode requires the hips itself;
    // under 'ignore' the same bone is only a missing bone, and only when the
    // caller requires it.
    openstrata::motion::RetargetMap noHips;
    assert(noHips.SetJointToken(openstrata::motion::HumanJoint::Spine, "Root/Pelvis/SpineA", skeleton));
    const openstrata::motion::RetargetDiagnostics underHips = openstrata::motion::DiagnoseRig(skeleton, noHips);
    assert(underHips.reported.front().subject == "hips");
    assert(underHips.reported.front().detail.find("root motion was dropped") != std::string::npos);
    assert(underHips.Subjects(Code::MissingRequiredBone) == std::vector<std::string>{"hips"});
    openstrata::motion::RetargetOptions ignore = DesignRequiredOptions();
    ignore.rootMotion.mode = openstrata::motion::RootMotionMode::Ignore;
    const openstrata::motion::RetargetDiagnostics underIgnore =
        openstrata::motion::DiagnoseRig(skeleton, noHips, ignore);
    assert(underIgnore.reported.front().subject == "hips");
    assert(underIgnore.reported.front().detail.find("root motion") == std::string::npos);
    openstrata::motion::RetargetOptions nothingRequired;
    nothingRequired.rootMotion.mode = openstrata::motion::RootMotionMode::Ignore;
    assert(openstrata::motion::DiagnoseRig(skeleton, noHips, nothingRequired).IsClean());
}

// RETARGETING_POLICY.md §4: which bones a target requires is the caller's
// statement. The same rig reports what each caller's set leaves unbound, in
// that set's order, and a caller with no set hears nothing about a bone it
// never asked for.
void
TestTheRequiredBonesAreTheCallersSet()
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    using J = openstrata::motion::HumanJoint;
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    const openstrata::motion::RetargetMap map = DesignMap(skeleton);

    assert(openstrata::motion::DiagnoseRig(skeleton, map).IsClean());

    openstrata::motion::RetargetOptions headFirst;
    headFirst.requiredBones = {J::Head, J::Hips, J::Neck};
    assert((openstrata::motion::DiagnoseRig(skeleton, map, headFirst)
                .Subjects(Code::MissingRequiredBone) == std::vector<std::string>{"head", "neck"}));
    assert((map.FindMissingRequiredBones(headFirst.requiredBones) == std::vector<J>{J::Head, J::Neck}));
    assert(map.FindMissingRequiredBones({}).empty());
}

// A clip whose every driven bone is bound reports exactly what the rig does, so
// a caller retargeting one pose at a time -- execVrm -- reaches the same list by
// asking DiagnoseRig once and each pose after it.
void
TestAClipReportsTheRigThenWhatItDrives()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    const openstrata::motion::RetargetMap map = DesignMap(skeleton);
    const openstrata::motion::RetargetOptions options = DesignRequiredOptions();
    const openstrata::motion::PoseRetargeter retargeter(skeleton, map, DesignSourceRest(), options);

    openstrata::motion::RetargetDiagnostics clip;
    retargeter.Retarget(DesignClip(), &clip);
    assert(!clip.IsClean());
    assert(clip == openstrata::motion::DiagnoseRig(skeleton, map, options));

    openstrata::motion::RetargetDiagnostics perPose =
        openstrata::motion::DiagnoseRig(skeleton, map, options);
    const openstrata::motion::MotionClip animation = DesignClip();
    for (const openstrata::motion::MotionPose& pose : animation.samples)
    {
        retargeter.Retarget(pose, &perPose);
    }
    assert(perPose == clip);
}

// A map built against a larger skeleton binds the hips to an index this rig
// does not have. The retarget can only drop the root there, so the rig reports
// the hips as missing for this rig -- with the dropped root in the detail --
// and the clip reports it once, not per sample. Before, the root was dropped
// without a word.
void
TestHipsBoundOutsideTheRigAreReportedWithTheDroppedRoot()
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    openstrata::motion::RetargetMap foreign = DesignMap(skeleton);
    assert(foreign.SetJointIndex(openstrata::motion::HumanJoint::Hips, 7, 10));
    assert(foreign.IsMapped(openstrata::motion::HumanJoint::Hips));

    const openstrata::motion::RetargetDiagnostics rig = openstrata::motion::DiagnoseRig(skeleton, foreign);
    assert(rig.reported.front().code == Code::MissingRequiredBone);
    assert(rig.reported.front().subject == "hips");
    assert(rig.reported.front().detail.find("root motion was dropped") != std::string::npos);

    const openstrata::motion::PoseRetargeter retargeter(skeleton, foreign, DesignSourceRest());
    openstrata::motion::RetargetDiagnostics clip;
    const openstrata::motion::RetargetedAnimation result = retargeter.Retarget(DesignClip(), &clip);
    // The rig's report, then the one thing the clip adds: it drives the hips,
    // which reach no joint -- exactly what an unmapped hips would report.
    assert(clip.Subjects(Code::MissingRequiredBone) == rig.Subjects(Code::MissingRequiredBone));
    assert(clip.Subjects(Code::UnboundDrivenBone) == std::vector<std::string>{"hips"});
    assert(clip.reported.size() == rig.reported.size() + 1);
    // Every joint keeps its rest translation: the root landed nowhere.
    assert(NearlyEqual(result.samples.back().translations[1], pxr::GfVec3f(0.0f, 1.0f, 0.0f)));

    // One pose at a time reaches the same single report.
    openstrata::motion::RetargetDiagnostics perPose;
    const openstrata::motion::MotionClip animation = DesignClip();
    for (const openstrata::motion::MotionPose& pose : animation.samples)
    {
        retargeter.Retarget(pose, &perPose);
    }
    assert(perPose.Subjects(Code::MissingRequiredBone) == std::vector<std::string>{"hips"});
}

// A bone the clip starts driving on a later sample is reported like one it
// drives from the first. Until P1-1 only the first sample was asked, so this
// bone went unreported.
void
TestABoneDrivenOnlyLaterIsStillReported()
{
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    const openstrata::motion::PoseRetargeter retargeter(skeleton, DesignMap(skeleton), DesignSourceRest());
    openstrata::motion::MotionClip animation = DesignClip();
    animation.samples.back().validRotations.set(static_cast<std::size_t>(openstrata::motion::HumanJoint::Jaw));

    openstrata::motion::RetargetDiagnostics diagnostics;
    retargeter.Retarget(animation, &diagnostics);
    assert(diagnostics.Subjects(openstrata::motion::RetargetDiagnosticCode::UnboundDrivenBone) ==
           std::vector<std::string>{"jaw"});
}

void
TestAJointsWorldTransformComposesItsWholeChain()
{
    // A look-at needs to know where the head *is*, and a RetargetedPose
    // states only where each joint sits relative to its parent. Composing that
    // is the piece between them, and the translation is the half that is easy
    // to get wrong: a parent's rotation has to turn the child's offset before
    // it is added, so a rotated spine moves the chest sideways rather than
    // further up.
    const openstrata::motion::SkeletonDescriptor skeleton = DesignAvatar();
    openstrata::motion::RetargetedPose pose;
    for (const openstrata::motion::SkeletonJoint& joint : skeleton.GetJoints())
    {
        pose.rotations.push_back(joint.restRotation);
        pose.translations.push_back(joint.restTranslation);
    }

    const int chest = skeleton.FindJoint("Root/Pelvis/SpineA/ChestA");
    assert(chest >= 0);
    pxr::GfQuatf orientation(1.0f);
    pxr::GfVec3f position(0.0f);
    assert(openstrata::motion::GetJointWorldTransform(skeleton, pose, chest, &orientation, &position));
    // 1 m to the pelvis plus two half-metre segments, all straight up.
    assert(NearlyEqual(position, pxr::GfVec3f(0.0f, 2.0f, 0.0f)));
    assert(SameOrientation(orientation, pxr::GfQuatf(1.0f)));

    // Tip the pelvis a quarter turn about +X: everything above it swings from
    // vertical onto +Z, and the pelvis's own metre of height stays.
    const int pelvis = skeleton.FindJoint("Root/Pelvis");
    pose.rotations[static_cast<std::size_t>(pelvis)] = Rotation(kAxisX, 90.0f);
    assert(openstrata::motion::GetJointWorldTransform(skeleton, pose, chest, &orientation, &position));
    assert(NearlyEqual(position, pxr::GfVec3f(0.0f, 1.0f, 1.0f)));
    assert(SameOrientation(orientation, Rotation(kAxisX, 90.0f)));

    // A pose that is not the skeleton's own width answers nothing rather than
    // reading past the end of it, and neither output is touched.
    openstrata::motion::RetargetedPose truncated = pose;
    truncated.rotations.pop_back();
    assert(
        !openstrata::motion::GetJointWorldTransform(skeleton, truncated, chest, &orientation, &position));
    assert(!openstrata::motion::GetJointWorldTransform(skeleton, pose, -1, &orientation, &position));
    assert(NearlyEqual(position, pxr::GfVec3f(0.0f, 1.0f, 1.0f)));
}

// RETARGETING_POLICY.md §2: a skeleton from tokens and rest matrices, the rule
// usd-vrm-plugins wrote twice. The decomposition and the parents are the
// library's own; what is new is the two refusals and the empty answer.
void
TestASkeletonIsBuiltFromTokensAndRestMatrices()
{
    using Error = openstrata::motion::SkeletonDescriptorError;
    pxr::GfMatrix4d pelvis(1.0);
    pelvis.SetTranslateOnly(pxr::GfVec3d(0.0, 1.0, 0.0));
    pxr::GfMatrix4d spine(1.0);
    spine.SetRotate(pxr::GfRotation(pxr::GfVec3d(0.0, 1.0, 0.0), 90.0));
    spine.SetTranslateOnly(pxr::GfVec3d(0.0, 0.5, 0.0));
    const std::vector<std::string> tokens = {"Root", "Root/Pelvis", "Root/Pelvis/SpineA"};
    const std::vector<pxr::GfMatrix4d> rests = {pxr::GfMatrix4d(1.0), pelvis, spine};

    const openstrata::motion::SkeletonDescriptorResult built =
        openstrata::motion::BuildSkeletonDescriptor(tokens, rests);
    assert(built.error == Error::None && built.skeleton);
    const std::vector<openstrata::motion::SkeletonJoint>& joints = built.skeleton->GetJoints();
    assert(joints.size() == 3);
    assert(joints[0].parent == openstrata::motion::SkeletonDescriptor::kNoParent);
    assert(joints[1].parent == 0 && joints[2].parent == 1);
    assert(NearlyEqual(joints[1].restTranslation, pxr::GfVec3f(0.0f, 1.0f, 0.0f)));
    assert(SameOrientation(joints[2].restRotation, Rotation(kAxisY, 90.0f)));
    // The same values the two library calls give by hand.
    openstrata::motion::SkeletonDescriptor byHand;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        openstrata::motion::SkeletonJoint joint;
        joint.token = tokens[i];
        openstrata::motion::DecomposeRestTransform(rests[i], &joint);
        byHand.AddJoint(joint);
    }
    byHand.ResolveParentsFromTokens();
    assert(*built.skeleton == byHand);

    // No tokens is the empty skeleton, even beside one stray rest.
    const auto empty = openstrata::motion::BuildSkeletonDescriptor({}, {pxr::GfMatrix4d(1.0)});
    assert(empty.error == Error::None && empty.skeleton && empty.skeleton->IsEmpty());

    const auto short_ = openstrata::motion::BuildSkeletonDescriptor(tokens, {pelvis});
    assert(short_.error == Error::RestTransformCount && !short_.skeleton);

    const auto blank = openstrata::motion::BuildSkeletonDescriptor({"Root", ""}, {pelvis, pelvis});
    assert(blank.error == Error::EmptyJointToken && !blank.skeleton);
}

// RETARGETING_POLICY.md §10: a clip's rest pose read off its semantic
// skeleton. A leaf is a bone, a parent is the bone its parent path's leaf
// names, and a joint that is no bone drops out of the chain.
void
TestASourceRestIsReadOffASemanticSkeleton()
{
    using J = openstrata::motion::HumanJoint;
    using Error = openstrata::motion::SourceRestPoseError;
    const auto slot = [](J bone) { return static_cast<std::size_t>(bone); };
    const std::size_t root = openstrata::motion::SourceRestPose::kNoParent;

    openstrata::motion::SkeletonDescriptor skeleton;
    const auto add = [&](const char* token, const pxr::GfQuatf& rotation, const pxr::GfVec3f& at) {
        openstrata::motion::SkeletonJoint joint;
        joint.token = token;
        joint.restRotation = rotation;
        joint.restTranslation = at;
        skeleton.AddJoint(joint);
    };
    const pxr::GfQuatf identity(1.0f);
    add("Root", Rotation(kAxisY, 45.0f), pxr::GfVec3f(0.0f));
    add("Root/hips", identity, pxr::GfVec3f(0.0f, 1.0f, 0.0f));
    add("Root/hips/spine", Rotation(kAxisX, 10.0f), pxr::GfVec3f(0.0f, 0.2f, 0.0f));
    add("Root/hips/spine/Twist", Rotation(kAxisZ, 30.0f), pxr::GfVec3f(0.0f));
    add("Root/hips/spine/Twist/chest", identity, pxr::GfVec3f(0.0f, 0.3f, 0.0f));
    // A parent path no joint resolves still parents the bone by its leaf.
    add("Elsewhere/neck", identity, pxr::GfVec3f(0.0f, 0.1f, 0.0f));
    skeleton.ResolveParentsFromTokens();

    const openstrata::motion::SourceRestPoseResult read =
        openstrata::motion::BuildSourceRestPose(skeleton);
    assert(read.error == Error::None && read.rest && read.offending.empty());
    const openstrata::motion::SourceRestPose& rest = *read.rest;
    assert(NearlyEqual(rest.localTranslations[slot(J::Hips)], pxr::GfVec3f(0.0f, 1.0f, 0.0f)));
    assert(SameOrientation(rest.localRotations[slot(J::Spine)], Rotation(kAxisX, 10.0f)));
    // `Root` is no bone, so the hips are a root and its turn is not theirs.
    assert(rest.parents[slot(J::Hips)] == root);
    assert(rest.parents[slot(J::Spine)] == slot(J::Hips));
    // `Twist` is no bone: chest has no semantic parent, and the twist's turn
    // is in nobody's slot.
    assert(rest.parents[slot(J::Chest)] == root);
    assert(SameOrientation(rest.GetWorldRestRotation(J::Chest), identity));
    assert(rest.parents[slot(J::Neck)] == root);
    assert(NearlyEqual(rest.localTranslations[slot(J::Neck)], pxr::GfVec3f(0.0f, 0.1f, 0.0f)));
    // Nothing named the head: it keeps the default.
    assert(SameOrientation(rest.localRotations[slot(J::Head)], identity));

    openstrata::motion::SkeletonDescriptor twice = skeleton;
    openstrata::motion::SkeletonJoint again;
    again.token = "Other/spine";
    twice.AddJoint(again);
    openstrata::motion::SkeletonJoint third;
    third.token = "Third/spine";
    twice.AddJoint(third);
    const auto duplicate = openstrata::motion::BuildSourceRestPose(twice);
    assert(duplicate.error == Error::DuplicateBone && !duplicate.rest);
    assert((duplicate.offending ==
            std::vector<std::pair<J, std::string>>{{J::Spine, "Root/hips/spine"},
                                                   {J::Spine, "Other/spine"},
                                                   {J::Spine, "Third/spine"}}));

    openstrata::motion::SkeletonDescriptor rig;
    openstrata::motion::SkeletonJoint pelvis;
    pelvis.token = "Root/Pelvis";
    rig.AddJoint(pelvis);
    const auto notSemantic = openstrata::motion::BuildSourceRestPose(rig);
    assert(notSemantic.error == Error::NoHumanBone && !notSemantic.rest);
}

} // namespace

int
main()
{
    TestSkeletonParentsComeFromJointPaths();
    TestARestTransformDecomposesItsScale();
    TestAMissingOptionalBoneIsNotAMissingBone();
    TestAHierarchyMismatchCarriesEachBoneRelativeToItsOwnParent();
    TestADuplicateMappingKeepsTheLaterDrivenBone();
    TestRetargetMapReportsGapsAndCollisions();
    TestRigValuesCompareExactly();
    TestARejectedRebindingUnmapsTheBone();
    TestIdentityRestPosesPassRotationsThrough();
    TestRestPoseCorrectionPreservesTheWorldDelta();
    TestRestPoseCorrectionAccountsForTheWholeAncestorChain();
    TestTargetReferenceRestIsSeparateFromUsdSkelRest();
    TestRootMotionModes();
    TestDesignTripletHandOff();
    TestUnmappedJointsStayAtRestAndAreReported();
    TestAClipIsRetargetedAtItsOwnSampleTimes();
    TestRootJointModeMovesTheReceiver();
    TestTheRetargetCodeTableIsClosedAndStable();
    TestARetargetDiagnosticFormatsOneStableLine();
    TestADiagnosticIsReportedOncePerCodeAndSubject();
    TestTheRigIsDiagnosedBeforeAnyClip();
    TestTheRequiredBonesAreTheCallersSet();
    TestAClipReportsTheRigThenWhatItDrives();
    TestABoneDrivenOnlyLaterIsStillReported();
    TestHipsBoundOutsideTheRigAreReportedWithTheDroppedRoot();
    TestAJointsWorldTransformComposesItsWholeChain();
    TestASkeletonIsBuiltFromTokensAndRestMatrices();
    TestASourceRestIsReadOffASemanticSkeleton();
    std::puts("motionRetarget unit tests passed");
    return 0;
}
