// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionRetarget/api.h"

#include "motionRetarget/SkeletonDescriptor.h"

#include "motionCore/MotionPose.h"

#include <array>
#include <bitset>
#include <string>
#include <vector>

namespace openstrata::motion
{

// Which target joint each human bone drives.
//
// The source of the mapping is the caller's problem: a format repository reads
// its own humanoid binding, a role table names a rig's conventional joints, a
// hand-written rig supplies a side file. Either way motionRetarget receives
// resolved indices and never guesses from joint names — name heuristics are
// exactly the kind of silent mis-retarget this contract exists to prevent
// (RETARGETING_POLICY.md §3).
class MOTIONRETARGET_API RetargetMap
{
  public:
    static constexpr int kUnmapped = -1;

    RetargetMap();

    // Binds `bone` to a target joint index. Returns false — leaving the bone
    // unmapped — when either the bone or the joint index is out of range, so a
    // rejected binding is never mistaken for a successful one.
    bool SetJointIndex(openstrata::motion::HumanJoint bone, int jointIndex, std::size_t jointCount);

    // Resolves `token` against `skeleton` and binds it. Returns false when the
    // skeleton has no such joint, leaving the bone unmapped -- including a
    // bone an earlier call had bound, as SetJointIndex does.
    bool SetJointToken(openstrata::motion::HumanJoint bone, const std::string& token,
                       const SkeletonDescriptor& skeleton);

    void Clear();

    // kUnmapped when the bone does not drive a joint of this rig.
    int GetJointIndex(openstrata::motion::HumanJoint bone) const;
    bool IsMapped(openstrata::motion::HumanJoint bone) const;
    std::size_t
    GetMappedCount() const noexcept
    {
        return _mapped.count();
    }

    // The bones of `required` with no binding, in the order `required` states
    // them. Which bones a target requires is the caller's statement, not the
    // vocabulary's (RETARGETING_POLICY.md §4): a format supplies its own set,
    // and a rig with no such rule supplies none.
    std::vector<openstrata::motion::HumanJoint>
    FindMissingRequiredBones(const std::vector<openstrata::motion::HumanJoint>& required) const;

    // True when two bones resolve to the same joint — always a mapping bug,
    // because the second binding would silently overwrite the first.
    std::vector<int> FindDuplicateJointIndices() const;

    // Exact: the same bones bound to the same joint indices. Like
    // SkeletonDescriptor's, it exists because `ExecTypeRegistry::RegisterType`
    // will not register a type it cannot compare (`execVrm`'s
    // `vrm.computeHumanoidMap`). Two maps are equal as *indices*, so maps built
    // against two different skeletons can compare equal -- the map never says
    // which rig its indices count into, and a consumer holds the skeleton
    // beside it.
    friend MOTIONRETARGET_API bool operator==(const RetargetMap& a, const RetargetMap& b) noexcept;
    friend MOTIONRETARGET_API bool operator!=(const RetargetMap& a, const RetargetMap& b) noexcept;

  private:
    std::array<int, openstrata::motion::HumanJointCount> _jointIndices;
    std::bitset<openstrata::motion::HumanJointCount> _mapped;
};

} // namespace openstrata::motion
