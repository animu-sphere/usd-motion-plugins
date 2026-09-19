// SPDX-License-Identifier: Apache-2.0
#include "motionRetarget/RestPose.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace openstrata::motion
{
namespace
{

const pxr::GfQuatf&
Identity()
{
    static const pxr::GfQuatf identity(1.0f, pxr::GfVec3f(0.0f));
    return identity;
}

// |real| rather than real: -1 is the same orientation as +1.
bool
IsIdentityRotation(const pxr::GfQuatf& q)
{
    const pxr::GfQuatf n = q.GetNormalized();
    return std::fabs(std::fabs(n.GetReal()) - 1.0f) <= 1e-6f;
}

// The bone a semantic joint path's leaf names, if it names one.
std::optional<openstrata::motion::HumanJoint>
BoneForLeaf(std::string_view jointPath)
{
    const std::size_t separator = jointPath.rfind('/');
    return openstrata::motion::FindHumanJoint(
        separator == std::string_view::npos ? jointPath : jointPath.substr(separator + 1));
}

} // namespace

SourceRestPose::SourceRestPose()
{
    localRotations.fill(Identity());
    localTranslations.fill(pxr::GfVec3f(0.0f));
    parents.fill(kNoParent);
}

void
SourceRestPose::SetParent(openstrata::motion::HumanJoint bone, openstrata::motion::HumanJoint parent)
{
    if (!openstrata::motion::IsValidHumanJoint(bone))
    {
        return;
    }
    parents[static_cast<std::size_t>(bone)] =
        openstrata::motion::IsValidHumanJoint(parent) ? static_cast<std::size_t>(parent) : kNoParent;
}

pxr::GfQuatf
SourceRestPose::GetWorldRestRotation(openstrata::motion::HumanJoint bone) const
{
    if (!openstrata::motion::IsValidHumanJoint(bone))
    {
        return Identity();
    }
    // world = L_root * ... * L_parent * L_bone, so each ancestor composes on
    // the left as the walk climbs. The depth cap makes a malformed `parents`
    // cycle terminate instead of spinning; a well-formed chain never revisits a
    // bone, so it cannot reach the cap.
    pxr::GfQuatf world = Identity();
    std::size_t cursor = static_cast<std::size_t>(bone);
    for (std::size_t depth = 0; depth < openstrata::motion::HumanJointCount && cursor < openstrata::motion::HumanJointCount;
         ++depth)
    {
        world = localRotations[cursor].GetNormalized() * world;
        cursor = parents[cursor];
    }
    return world.GetNormalized();
}

RestPoseCorrection::RestPoseCorrection()
{
    pre.fill(Identity());
    post.fill(Identity());
    identity.fill(true);
}

pxr::GfQuatf
RestPoseCorrection::Apply(openstrata::motion::HumanJoint bone, const pxr::GfQuatf& rotation) const
{
    if (!openstrata::motion::IsValidHumanJoint(bone))
    {
        return rotation;
    }
    const auto slot = static_cast<std::size_t>(bone);
    if (identity[slot])
    {
        return rotation;
    }
    return (pre[slot] * rotation * post[slot]).GetNormalized();
}

bool
operator==(const RestPoseCorrection& a, const RestPoseCorrection& b) noexcept
{
    return a.identity == b.identity && a.pre == b.pre && a.post == b.post;
}

bool
operator!=(const RestPoseCorrection& a, const RestPoseCorrection& b) noexcept
{
    return !(a == b);
}

RestPoseCorrection
ComputeRestPoseCorrection(const SourceRestPose& source, const SkeletonDescriptor& target,
                          const RetargetMap& map)
{
    RestPoseCorrection correction;
    const std::vector<SkeletonJoint>& joints = target.GetJoints();

    for (std::size_t slot = 0; slot < openstrata::motion::HumanJointCount; ++slot)
    {
        const auto bone = static_cast<openstrata::motion::HumanJoint>(slot);
        const int jointIndex = map.GetJointIndex(bone);
        if (jointIndex < 0 || static_cast<std::size_t>(jointIndex) >= joints.size())
        {
            continue;
        }

        // Sp and Tp are the parents' *accumulated* rest rotations. Using each
        // parent's own local rotation would agree only where the parent is
        // itself a root, and would silently mis-retarget every bone below the
        // second level of a rig whose rest pose is not identity.
        const pxr::GfQuatf sourceRest = source.localRotations[slot].GetNormalized();
        const std::size_t sourceParent = source.parents[slot];
        const pxr::GfQuatf sourceParentRest =
            sourceParent < openstrata::motion::HumanJointCount
                ? source.GetWorldRestRotation(static_cast<openstrata::motion::HumanJoint>(sourceParent))
                : Identity();

        const SkeletonJoint& joint = joints[static_cast<std::size_t>(jointIndex)];
        const pxr::GfQuatf targetRest = joint.restRotation.GetNormalized();
        const pxr::GfQuatf targetParentRest = target.GetWorldRestRotation(joint.parent);

        if (IsIdentityRotation(sourceRest) && IsIdentityRotation(sourceParentRest) &&
            IsIdentityRotation(targetRest) && IsIdentityRotation(targetParentRest))
        {
            continue;
        }

        // Qt = (Tp^-1 * Sp) * Qs * (S^-1 * Sp^-1 * Tp * T); see RestPose.h.
        correction.pre[slot] = (targetParentRest.GetInverse() * sourceParentRest).GetNormalized();
        correction.post[slot] = (sourceRest.GetInverse() * sourceParentRest.GetInverse() *
                                 targetParentRest * targetRest)
                                    .GetNormalized();
        correction.identity[slot] = false;
    }

    return correction;
}

SourceRestPoseResult
BuildSourceRestPose(const SkeletonDescriptor& semanticSkeleton)
{
    SourceRestPoseResult result;
    SourceRestPose rest;

    // Which joint first named each bone, so a second naming can report both.
    std::array<const std::string*, openstrata::motion::HumanJointCount> namedBy{};
    std::size_t recognized = 0;

    for (const SkeletonJoint& joint : semanticSkeleton.GetJoints())
    {
        const std::optional<openstrata::motion::HumanJoint> bone = BoneForLeaf(joint.token);
        if (!bone)
        {
            continue;
        }
        const auto slot = static_cast<std::size_t>(*bone);
        if (namedBy[slot])
        {
            // Report the first naming once, then every later one.
            const bool firstReported =
                std::any_of(result.offending.begin(), result.offending.end(),
                            [&](const auto& named) { return named.first == *bone; });
            if (!firstReported)
            {
                result.offending.emplace_back(*bone, *namedBy[slot]);
            }
            result.offending.emplace_back(*bone, joint.token);
            continue;
        }
        namedBy[slot] = &joint.token;
        ++recognized;

        rest.localRotations[slot] = joint.restRotation;
        rest.localTranslations[slot] = joint.restTranslation;
        const std::size_t separator = joint.token.rfind('/');
        if (separator == std::string::npos)
        {
            continue;
        }
        if (const std::optional<openstrata::motion::HumanJoint> parent =
                BoneForLeaf(std::string_view(joint.token).substr(0, separator)))
        {
            rest.SetParent(*bone, *parent);
        }
    }

    if (!result.offending.empty())
    {
        result.error = SourceRestPoseError::DuplicateBone;
        return result;
    }
    if (recognized == 0)
    {
        result.error = SourceRestPoseError::NoHumanBone;
        return result;
    }
    result.rest = std::move(rest);
    return result;
}

} // namespace openstrata::motion
