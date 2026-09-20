// SPDX-License-Identifier: Apache-2.0
#include "motionCore/MotionPose.h"

#include <algorithm>
#include <array>
#include <utility>

namespace openstrata::motion
{
namespace
{

constexpr std::array<std::string_view, HumanJointCount> kHumanBoneNames = {
    "hips",
    "spine",
    "chest",
    "upperChest",
    "neck",
    "head",
    "leftEye",
    "rightEye",
    "jaw",
    "leftUpperLeg",
    "leftLowerLeg",
    "leftFoot",
    "leftToes",
    "rightUpperLeg",
    "rightLowerLeg",
    "rightFoot",
    "rightToes",
    "leftShoulder",
    "leftUpperArm",
    "leftLowerArm",
    "leftHand",
    "rightShoulder",
    "rightUpperArm",
    "rightLowerArm",
    "rightHand",
    "leftThumbMetacarpal",
    "leftThumbProximal",
    "leftThumbDistal",
    "leftIndexProximal",
    "leftIndexIntermediate",
    "leftIndexDistal",
    "leftMiddleProximal",
    "leftMiddleIntermediate",
    "leftMiddleDistal",
    "leftRingProximal",
    "leftRingIntermediate",
    "leftRingDistal",
    "leftLittleProximal",
    "leftLittleIntermediate",
    "leftLittleDistal",
    "rightThumbMetacarpal",
    "rightThumbProximal",
    "rightThumbDistal",
    "rightIndexProximal",
    "rightIndexIntermediate",
    "rightIndexDistal",
    "rightMiddleProximal",
    "rightMiddleIntermediate",
    "rightMiddleDistal",
    "rightRingProximal",
    "rightRingIntermediate",
    "rightRingDistal",
    "rightLittleProximal",
    "rightLittleIntermediate",
    "rightLittleDistal",
};

static_assert(kHumanBoneNames.size() == HumanJointCount,
              "HumanJoint names must cover the complete enum");

} // namespace

bool
IsValidHumanJoint(HumanJoint joint) noexcept
{
    return static_cast<std::size_t>(joint) < HumanJointCount;
}

std::string_view
HumanJointName(HumanJoint joint) noexcept
{
    if (!IsValidHumanJoint(joint))
    {
        return {};
    }
    return kHumanBoneNames[static_cast<std::size_t>(joint)];
}

std::optional<HumanJoint>
FindHumanJoint(std::string_view name) noexcept
{
    for (std::size_t index = 0; index != kHumanBoneNames.size(); ++index)
    {
        if (kHumanBoneNames[index] == name)
        {
            return static_cast<HumanJoint>(index);
        }
    }
    return std::nullopt;
}

std::optional<HumanJoint>
HumanJointParent(HumanJoint joint) noexcept
{
    using Joint = HumanJoint;
    switch (joint)
    {
    case Joint::Hips:
        return std::nullopt;
    case Joint::Spine:
        return Joint::Hips;
    case Joint::Chest:
        return Joint::Spine;
    case Joint::UpperChest:
        return Joint::Chest;
    case Joint::Neck:
        return Joint::UpperChest;
    case Joint::Head:
        return Joint::Neck;
    case Joint::LeftEye:
    case Joint::RightEye:
    case Joint::Jaw:
        return Joint::Head;

    case Joint::LeftUpperLeg:
    case Joint::RightUpperLeg:
        return Joint::Hips;
    case Joint::LeftLowerLeg:
        return Joint::LeftUpperLeg;
    case Joint::LeftFoot:
        return Joint::LeftLowerLeg;
    case Joint::LeftToes:
        return Joint::LeftFoot;
    case Joint::RightLowerLeg:
        return Joint::RightUpperLeg;
    case Joint::RightFoot:
        return Joint::RightLowerLeg;
    case Joint::RightToes:
        return Joint::RightFoot;

    case Joint::LeftShoulder:
    case Joint::RightShoulder:
        return Joint::UpperChest;
    case Joint::LeftUpperArm:
        return Joint::LeftShoulder;
    case Joint::LeftLowerArm:
        return Joint::LeftUpperArm;
    case Joint::LeftHand:
        return Joint::LeftLowerArm;
    case Joint::RightUpperArm:
        return Joint::RightShoulder;
    case Joint::RightLowerArm:
        return Joint::RightUpperArm;
    case Joint::RightHand:
        return Joint::RightLowerArm;

    case Joint::LeftThumbMetacarpal:
    case Joint::LeftIndexProximal:
    case Joint::LeftMiddleProximal:
    case Joint::LeftRingProximal:
    case Joint::LeftLittleProximal:
        return Joint::LeftHand;
    case Joint::LeftThumbProximal:
        return Joint::LeftThumbMetacarpal;
    case Joint::LeftThumbDistal:
        return Joint::LeftThumbProximal;
    case Joint::LeftIndexIntermediate:
        return Joint::LeftIndexProximal;
    case Joint::LeftIndexDistal:
        return Joint::LeftIndexIntermediate;
    case Joint::LeftMiddleIntermediate:
        return Joint::LeftMiddleProximal;
    case Joint::LeftMiddleDistal:
        return Joint::LeftMiddleIntermediate;
    case Joint::LeftRingIntermediate:
        return Joint::LeftRingProximal;
    case Joint::LeftRingDistal:
        return Joint::LeftRingIntermediate;
    case Joint::LeftLittleIntermediate:
        return Joint::LeftLittleProximal;
    case Joint::LeftLittleDistal:
        return Joint::LeftLittleIntermediate;

    case Joint::RightThumbMetacarpal:
    case Joint::RightIndexProximal:
    case Joint::RightMiddleProximal:
    case Joint::RightRingProximal:
    case Joint::RightLittleProximal:
        return Joint::RightHand;
    case Joint::RightThumbProximal:
        return Joint::RightThumbMetacarpal;
    case Joint::RightThumbDistal:
        return Joint::RightThumbProximal;
    case Joint::RightIndexIntermediate:
        return Joint::RightIndexProximal;
    case Joint::RightIndexDistal:
        return Joint::RightIndexIntermediate;
    case Joint::RightMiddleIntermediate:
        return Joint::RightMiddleProximal;
    case Joint::RightMiddleDistal:
        return Joint::RightMiddleIntermediate;
    case Joint::RightRingIntermediate:
        return Joint::RightRingProximal;
    case Joint::RightRingDistal:
        return Joint::RightRingIntermediate;
    case Joint::RightLittleIntermediate:
        return Joint::RightLittleProximal;
    case Joint::RightLittleDistal:
        return Joint::RightLittleIntermediate;

    case Joint::Count:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<HumanJoint>
NearestPresentAncestor(HumanJoint joint, const std::bitset<HumanJointCount>& present) noexcept
{
    std::optional<HumanJoint> parent = HumanJointParent(joint);
    while (parent)
    {
        if (present.test(static_cast<std::size_t>(*parent)))
        {
            return parent;
        }
        parent = HumanJointParent(*parent);
    }
    return std::nullopt;
}

std::optional<HumanJoint>
FindHumanJointByPath(std::string_view path) noexcept
{
    const std::size_t separator = path.rfind('/');
    const std::string_view leaf =
        separator == std::string_view::npos ? path : path.substr(separator + 1);
    if (leaf.empty())
    {
        return std::nullopt;
    }
    return FindHumanJoint(leaf);
}

std::string
HumanJointPath(HumanJoint joint, const std::bitset<HumanJointCount>& present)
{
    if (!IsValidHumanJoint(joint))
    {
        return {};
    }
    // Every joint's parent has a smaller enum value, so walking up terminates.
    std::string path(HumanJointName(joint));
    std::optional<HumanJoint> ancestor = NearestPresentAncestor(joint, present);
    while (ancestor)
    {
        path.insert(0, "/");
        path.insert(0, HumanJointName(*ancestor));
        ancestor = NearestPresentAncestor(*ancestor, present);
    }
    return path;
}

MotionPose::MotionPose()
{
    // Exported rather than inline so every consumer receives the same identity
    // defaults across the motionCore DLL boundary.
    localRotations.fill(pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f)));
}

bool
MotionChannelSet::Set(std::string_view name, float value)
{
    const auto position = std::lower_bound(
        entries.begin(), entries.end(), name,
        [](const MotionChannel& entry, std::string_view sought) { return entry.name < sought; });
    if (position != entries.end() && position->name == name)
    {
        position->value = value;
        return false;
    }
    MotionChannel entry;
    entry.name.assign(name);
    entry.value = value;
    entries.insert(position, std::move(entry));
    return true;
}

const float*
MotionChannelSet::Find(std::string_view name) const noexcept
{
    const auto position = std::lower_bound(
        entries.begin(), entries.end(), name,
        [](const MotionChannel& entry, std::string_view sought) { return entry.name < sought; });
    return position != entries.end() && position->name == name ? &position->value : nullptr;
}

} // namespace openstrata::motion
