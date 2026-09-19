// SPDX-License-Identifier: Apache-2.0
//
// A target rig described as plain values.
//
// This is deliberately not a UsdSkelSkeleton. motionRetarget never opens a stage:
// the caller reads the skeleton off the stage and hands the values in, so the
// retarget core stays testable without USD composition and reusable by a live
// source that has no stage at all (WORKSPACE.md §2).
#pragma once

#include "motionRetarget/api.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec3f.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openstrata::motion
{

// One joint of the target rig, in the rig's own joint order.
struct SkeletonJoint
{
    // The joint's token exactly as it appears in UsdSkelSkeleton.joints — a
    // full joint path such as "Root/Pelvis/SpineA", not the leaf name.
    std::string token;

    // Index into SkeletonDescriptor::joints, or kNoParent for a root joint.
    int parent = -1;

    // Rest transform, decomposed (DecomposeRestTransform below).
    pxr::GfQuatf restRotation = pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f));
    pxr::GfVec3f restTranslation = pxr::GfVec3f(0.0f);

    // The rest's scale, per axis. The retarget never reads it: rotations and
    // translations are retargeted, scale is not animated. It is carried because
    // a bake states every joint whole, and UsdSkel takes an animated joint's
    // local transform from the animation alone -- so a bake that authored
    // identity would *replace* a scaled rest rather than keep it (the scale
    // policy, docs/design/MOTION_CONTRACT.md "Scale policy").
    pxr::GfVec3f restScale = pxr::GfVec3f(1.0f);
};

// Decomposes a UsdSkelSkeleton rest transform into `joint`'s three rest
// fields: the translation straight off the matrix, the rotation from what is
// left once scale and shear are removed, and the scale as the length of each
// basis row. One implementation, because `motion_retarget` and `execVrm` each
// carried a copy and a parity difference in a normalization step would be one
// P0-6 has to explain rather than measure. `token` and `parent` are untouched.
MOTIONRETARGET_API void DecomposeRestTransform(const pxr::GfMatrix4d& matrix, SkeletonJoint* joint);

class MOTIONRETARGET_API SkeletonDescriptor
{
  public:
    static constexpr int kNoParent = -1;

    SkeletonDescriptor() = default;
    explicit SkeletonDescriptor(std::vector<SkeletonJoint> joints) : _joints(std::move(joints))
    {
    }

    const std::vector<SkeletonJoint>&
    GetJoints() const noexcept
    {
        return _joints;
    }
    std::size_t
    GetSize() const noexcept
    {
        return _joints.size();
    }
    bool
    IsEmpty() const noexcept
    {
        return _joints.empty();
    }

    void
    AddJoint(const SkeletonJoint& joint)
    {
        _joints.push_back(joint);
    }

    // Returns the joint's index, or kNoParent when no joint carries the token.
    // Matching is exact on the full joint path.
    int FindJoint(const std::string& token) const;

    // Derives `parent` for every joint from the "a/b/c" joint-path convention
    // UsdSkelSkeleton uses. A joint whose parent path is absent from the
    // skeleton is treated as a root. Call this after populating tokens when the
    // source did not supply parent indices.
    void ResolveParentsFromTokens();

    // True when every parent index is either kNoParent or a strictly smaller
    // index — UsdSkelSkeleton requires parents to precede their children.
    bool IsTopologicallyOrdered() const;

    // The joint's rest orientation in skeleton space: its own rest rotation
    // with every ancestor's composed on the left, root-first. kNoParent — or
    // any out-of-range index — yields identity, which is exactly what a root
    // joint's absent parent contributes to a rest-pose correction.
    pxr::GfQuatf GetWorldRestRotation(int jointIndex) const;

  private:
    std::vector<SkeletonJoint> _joints;
};

// Exact, field by field and joint by joint -- motionCore's "is this the same
// recorded value?" question (motion contract, comparison semantics), asked of a
// rig. It exists for the caller motionCore's aggregates and motionRuntime's
// `PoseSampleResult` added it for: `ExecTypeRegistry::RegisterType` will not
// register a type it cannot compare, and `execVrm`'s `vrm.computeTargetSkeleton`
// hands this value back whole.
//
// Exact means a rest rotation and its negation are *different* skeletons here,
// though they rest identically. Downstream of an exec computation that is the
// conservative answer -- a flipped sign recomputes what depends on it, which is
// wasteful and never wrong -- and it is the same one `MotionPose` gives. There
// is no `NearlyEqual`: nothing yet asks whether two rigs are the same rig, and a
// parity check compares the poses retargeted onto them.
MOTIONRETARGET_API bool operator==(const SkeletonJoint& a, const SkeletonJoint& b) noexcept;
MOTIONRETARGET_API bool operator!=(const SkeletonJoint& a, const SkeletonJoint& b) noexcept;
MOTIONRETARGET_API bool operator==(const SkeletonDescriptor& a, const SkeletonDescriptor& b) noexcept;
MOTIONRETARGET_API bool operator!=(const SkeletonDescriptor& a, const SkeletonDescriptor& b) noexcept;

// Why joint tokens and rest transforms describe no skeleton.
enum class SkeletonDescriptorError : std::uint8_t
{
    None,
    // The rest transforms do not pair one-to-one with a token list that names
    // at least one joint. A rest pose cannot be computed for such a skeleton,
    // and inventing identity for the missing ones would be numbers nobody can
    // tell from measured ones.
    RestTransformCount,
    // A token is empty. It names no joint path, and no UsdSkelSkeleton has
    // one.
    EmptyJointToken,
};

struct SkeletonDescriptorResult
{
    // Set exactly when `error` is None.
    std::optional<SkeletonDescriptor> skeleton;
    SkeletonDescriptorError error = SkeletonDescriptorError::None;
};

// A skeleton from what a UsdSkelSkeleton states: its joint tokens and one rest
// transform per token (RETARGETING_POLICY.md §2). Each rest is decomposed by
// DecomposeRestTransform, and each parent is derived from the tokens by
// ResolveParentsFromTokens, the "a/b/c" rule.
//
// No tokens is the empty skeleton, whatever `restTransforms` holds: with no
// joint for a rest transform to belong to, none can become a number. That is
// an answer, not a refusal -- an empty skeleton says exactly what was stated,
// and a map built against it binds nothing.
//
// It takes values, not a skeleton prim, so a caller that reads them off a
// stage (motionUsd, a format repository, an OpenExec node) and one that has no
// stage share one rule. usd-vrm-plugins carried two copies of it, one in a CLI
// and one in an exec bundle; this is where they meet.
MOTIONRETARGET_API SkeletonDescriptorResult
BuildSkeletonDescriptor(const std::vector<std::string>& jointTokens,
                        const std::vector<pxr::GfMatrix4d>& restTransforms);

} // namespace openstrata::motion
