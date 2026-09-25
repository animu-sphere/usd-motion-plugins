// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionRetarget/api.h"

#include "motionRetarget/RetargetMap.h"
#include "motionRetarget/SkeletonDescriptor.h"

#include "motionCore/MotionPose.h"

#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec3f.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace openstrata::motion
{

// The clip's own rest pose, per human bone. `usdVrmaFileFormat` authors a
// semantic skeleton whose rest rotations are all identity, but a clip from
// another producer need not, so the correction is computed rather than assumed.
struct SourceRestPose
{
    MOTIONRETARGET_API SourceRestPose();

    // Marks a bone with no semantic parent.
    static constexpr std::size_t kNoParent = openstrata::motion::HumanJointCount;

    std::array<pxr::GfQuatf, openstrata::motion::HumanJointCount> localRotations;
    std::array<pxr::GfVec3f, openstrata::motion::HumanJointCount> localTranslations;

    // Semantic parent of each bone, as an index into the arrays above, or
    // kNoParent. The default is "every bone is a root", which is exactly right
    // for an all-identity rest pose (an identity parent contributes nothing to
    // the correction) and keeps this library free of a second copy of the VRM
    // humanoid taxonomy. A producer with a non-identity rest pose fills it in.
    std::array<std::size_t, openstrata::motion::HumanJointCount> parents;

    MOTIONRETARGET_API void SetParent(openstrata::motion::HumanJoint bone, openstrata::motion::HumanJoint parent);

    // The bone's rest orientation in the clip's own root space: its local rest
    // rotation with every ancestor's composed on the left, root-first. The
    // correction below is a world-space identity, so an ancestor two levels up
    // contributes even though it is not the bone's parent — reading the
    // parent's *local* rotation instead is only right when the parent is
    // itself a root.
    MOTIONRETARGET_API pxr::GfQuatf GetWorldRestRotation(openstrata::motion::HumanJoint bone) const;
};

// Why a skeleton could not be read as a clip's rest pose.
enum class SourceRestPoseError : std::uint8_t
{
    None,
    // No joint's leaf is a bone of the vocabulary. The skeleton is not a
    // semantic one, and the rest pose read off it would be the default -- every
    // bone at identity -- which is numbers nobody can tell from a measured rest.
    NoHumanBone,
    // Two joints' leaves name the same bone, and which of the two rests the
    // clip meant cannot be known.
    DuplicateBone,
};

struct SourceRestPoseResult
{
    // Set exactly when `error` is None.
    std::optional<SourceRestPose> rest;
    SourceRestPoseError error = SourceRestPoseError::None;

    // For DuplicateBone: every joint token that named a bone another joint also
    // named, beside that bone -- the first naming once, then each later one.
    std::vector<std::pair<openstrata::motion::HumanJoint, std::string>> offending;
};

// A clip's rest pose, per bone, read off its semantic skeleton
// (RETARGETING_POLICY.md §10).
//
// On a semantic skeleton a joint's leaf *is* its bone: that is what the motion
// contract says a canonical clip's skeleton is, not a name heuristic, and it is
// never applied to a target rig, whose bones are the caller's bindings. For
// each joint whose leaf is a vocabulary name, its decomposed rest rotation and
// translation fill that bone's slot, and its semantic parent is the bone named
// by the leaf of its parent *path* -- the path, whether or not a joint of the
// skeleton resolves it. A joint whose leaf is no bone contributes nothing, and
// a bone whose parent path's leaf is no bone is a root, so a non-bone joint
// between two bones drops out of the chain: SourceRestPose has one slot per
// bone and no other.
//
// usd-vrm-plugins read it this way in a CLI and copied it line for line into
// an exec bundle; this is where the two meet.
MOTIONRETARGET_API SourceRestPoseResult BuildSourceRestPose(const SkeletonDescriptor& semanticSkeleton);

// Optional humanoid reference rest for a target rig. Slots use the target's
// joint order; an unset slot uses SkeletonJoint::restRotation. This stays
// separate from the UsdSkel rest, which is also the pose of an undriven joint.
// Stating local rotations lets an override on an ancestor contribute to every
// descendant's reference world rotation without changing the skeleton.
struct TargetRestPose
{
    std::vector<std::optional<pxr::GfQuatf>> localRotations;

    // An unset slot uses the skeleton rest; an invalid index returns identity.
    MOTIONRETARGET_API pxr::GfQuatf GetLocalRestRotation(const SkeletonDescriptor& skeleton,
                                                       int jointIndex) const;
    MOTIONRETARGET_API pxr::GfQuatf GetWorldRestRotation(const SkeletonDescriptor& skeleton,
                                                       int jointIndex) const;
};

// Per-bone correction carrying a rest-relative rotation from the source rig
// onto a target whose rest pose differs.
//
// The invariant is that the bone's world-space rotation *away from its own
// rest* is preserved. With source local rest `S`, target local rest `T`, and
// `Sp`/`Tp` the *accumulated* rest rotations of each parent chain — not the
// parent's own local rotation — equating the two world deltas
//
//     Tp * Qt * T^-1 * Tp^-1  ==  Sp * Qs * S^-1 * Sp^-1
//
// gives
//
//     Qt = (Tp^-1 * Sp) * Qs * (S^-1 * Sp^-1 * Tp * T)   [= pre * Qs * post]
//
// Composition is OpenUSD's: `a * b` applies `b` first. Where both rest poses
// are identity — the `usdVrmaFileFormat` case — pre and post are identity and
// the sample passes through untouched.
struct RestPoseCorrection
{
    MOTIONRETARGET_API RestPoseCorrection();

    std::array<pxr::GfQuatf, openstrata::motion::HumanJointCount> pre;
    std::array<pxr::GfQuatf, openstrata::motion::HumanJointCount> post;
    std::array<bool, openstrata::motion::HumanJointCount> identity;

    // Returns `rotation` unchanged when the bone's correction is identity.
    MOTIONRETARGET_API pxr::GfQuatf Apply(openstrata::motion::HumanJoint bone, const pxr::GfQuatf& rotation) const;
};

// Exact, bone by bone: the same `pre`, `post` and `identity` in every slot. It
// exists for the caller `SkeletonDescriptor`'s and `RetargetMap`'s did:
// `ExecTypeRegistry::RegisterType` will not register a type it cannot compare,
// and `execVrm`'s `vrm.computeRestPoseCorrection` hands this value back whole.
//
// Exact means a correction and its negation are *different* values although
// they apply identically, which is the conservative answer downstream of an
// exec computation and the one `SkeletonDescriptor` gives. The `identity` flags are
// compared too: they are what `Apply` reads first, so two corrections that
// differ only there answer differently for the same rotation.
MOTIONRETARGET_API bool operator==(const RestPoseCorrection& a, const RestPoseCorrection& b) noexcept;
MOTIONRETARGET_API bool operator!=(const RestPoseCorrection& a, const RestPoseCorrection& b) noexcept;

// Builds the correction for every mapped bone; unmapped bones stay identity.
MOTIONRETARGET_API RestPoseCorrection ComputeRestPoseCorrection(const SourceRestPose& source,
                                                             const SkeletonDescriptor& target,
                                                             const RetargetMap& map);
MOTIONRETARGET_API RestPoseCorrection ComputeRestPoseCorrection(const SourceRestPose& source,
                                                             const SkeletonDescriptor& target,
                                                             const RetargetMap& map,
                                                             const TargetRestPose& targetRest);

} // namespace openstrata::motion
