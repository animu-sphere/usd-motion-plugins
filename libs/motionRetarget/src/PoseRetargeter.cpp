// SPDX-License-Identifier: Apache-2.0
#include "motionRetarget/PoseRetargeter.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace openstrata::motion
{
namespace
{

std::string
Describe(openstrata::motion::HumanJoint bone)
{
    return std::string(openstrata::motion::HumanJointName(bone));
}

// The one required bone whose absence costs more than its own motion: under
// the default root-motion mode the hips are where the root lands, so a rig
// without them drops the body's translation as well.
std::string
MissingRequiredDetail(openstrata::motion::HumanJoint bone, const RootMotionOptions& options)
{
    if (bone == openstrata::motion::HumanJoint::Hips && options.mode == RootMotionMode::Hips)
    {
        return "the target rig binds no joint for this required bone, and "
               "root-motion mode 'hips' lands the root on it, so root motion "
               "was dropped";
    }
    return "the target rig binds no joint for this required bone";
}

void
ReportInvalidRootJoint(const RootMotionOptions& options, std::size_t jointCount,
                       RetargetDiagnostics* diagnostics)
{
    const std::string subject = std::to_string(options.rootJointIndex);
    if (diagnostics->Has(RetargetDiagnosticCode::InvalidRootJoint, subject))
    {
        return;
    }
    diagnostics->Report(MakeRetargetDiagnostic(
        RetargetDiagnosticCode::InvalidRootJoint, subject,
        "root-motion mode 'root' names this joint index and the target rig "
        "has " +
            std::to_string(jointCount) + " joints, so no root translation was authored"));
}

bool
IsValidRootJoint(const RootMotionOptions& options, std::size_t jointCount)
{
    return options.rootJointIndex >= 0 &&
           static_cast<std::size_t>(options.rootJointIndex) < jointCount;
}

} // namespace

bool
operator==(const RetargetedPose& a, const RetargetedPose& b) noexcept
{
    return a.timestamp == b.timestamp && a.rotations == b.rotations &&
           a.translations == b.translations;
}

bool
operator!=(const RetargetedPose& a, const RetargetedPose& b) noexcept
{
    return !(a == b);
}

bool
operator==(const JointLocalTransforms& a, const JointLocalTransforms& b) noexcept
{
    return a.timestamp == b.timestamp && a.joints == b.joints && a.translations == b.translations &&
           a.rotations == b.rotations && a.scales == b.scales;
}

bool
operator!=(const JointLocalTransforms& a, const JointLocalTransforms& b) noexcept
{
    return !(a == b);
}

bool
GetJointWorldTransform(const SkeletonDescriptor& skeleton, const RetargetedPose& pose, int jointIndex,
                       pxr::GfQuatf* orientation, pxr::GfVec3f* position)
{
    const std::vector<SkeletonJoint>& joints = skeleton.GetJoints();
    const std::size_t count = joints.size();
    if (jointIndex < 0 || static_cast<std::size_t>(jointIndex) >= count ||
        pose.rotations.size() != count || pose.translations.size() != count)
    {
        return false;
    }

    // Root-first, because composing a chain means applying the outermost
    // ancestor's transform last. The bound is the joint count: a chain longer
    // than that has revisited a joint, which is a cycle -- and a rig whose
    // parents cycle is one this refuses rather than walks forever.
    std::vector<int> chain;
    chain.reserve(count);
    for (int walk = jointIndex; walk != SkeletonDescriptor::kNoParent;
         walk = joints[static_cast<std::size_t>(walk)].parent)
    {
        if (walk < 0 || static_cast<std::size_t>(walk) >= count || chain.size() == count)
        {
            return false;
        }
        chain.push_back(walk);
    }

    pxr::GfQuatf composedRotation(1.0f);
    pxr::GfVec3f composedPosition(0.0f);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        const auto slot = static_cast<std::size_t>(*it);
        composedPosition += composedRotation.Transform(pose.translations[slot]);
        composedRotation = (composedRotation * pose.rotations[slot]).GetNormalized();
    }

    if (orientation)
    {
        *orientation = composedRotation;
    }
    if (position)
    {
        *position = composedPosition;
    }
    return true;
}

PoseRetargeter::PoseRetargeter(SkeletonDescriptor skeleton, RetargetMap map, SourceRestPose sourceRest,
                               RetargetOptions options)
    : _skeleton(std::move(skeleton)), _map(std::move(map)), _sourceRest(std::move(sourceRest)),
      _options(std::move(options)),
      _correction(ComputeRestPoseCorrection(_sourceRest, _skeleton, _map, _options.targetRest))
{
    if (_options.foldUnboundIntermediateRotations) {
        _BuildFoldPlan();
    }
}

void
PoseRetargeter::_BuildFoldPlan()
{
    constexpr std::size_t count = HumanJointCount;
    const auto& joints = _skeleton.GetJoints();
    const auto bound = [&](std::size_t slot) {
        const int index = _map.GetJointIndex(static_cast<HumanJoint>(slot));
        return index >= 0 && static_cast<std::size_t>(index) < joints.size();
    };
    // A malformed chain or a colliding map cannot identify a receiver safely.
    if (!_map.FindDuplicateJointIndices().empty()) {
        return;
    }
    for (std::size_t index = 0; index < joints.size(); ++index) {
        if (joints[index].parent < SkeletonDescriptor::kNoParent ||
            joints[index].parent >= static_cast<int>(index)) {
            return;
        }
    }
    for (std::size_t slot = 0; slot < count; ++slot) {
        if (_map.IsMapped(static_cast<HumanJoint>(slot)) && !bound(slot)) {
            return;
        }
    }
    std::array<bool, count> valid{};
    std::array<std::size_t, count> depths{};
    for (std::size_t slot = 0; slot < count; ++slot) {
        std::size_t walk = slot;
        std::bitset<count> visited;
        while (walk < count && !visited.test(walk)) {
            visited.set(walk);
            ++depths[slot];
            walk = _sourceRest.parents[walk];
        }
        valid[slot] = walk == SourceRestPose::kNoParent;
    }
    for (std::size_t slot = 0; slot < count; ++slot) {
        if (bound(slot) || !valid[slot]) {
            continue;
        }
        std::size_t ancestor = _sourceRest.parents[slot];
        while (ancestor < count && !bound(ancestor)) {
            ancestor = _sourceRest.parents[ancestor];
        }
        if (ancestor == SourceRestPose::kNoParent) {
            continue;
        }
        const int receiver = _map.GetJointIndex(static_cast<HumanJoint>(ancestor));
        bool haveDescendant = false;
        bool compatible = true;
        for (std::size_t child = 0; child < count; ++child) {
            if (!bound(child) || !valid[child]) {
                continue;
            }
            std::size_t walk = _sourceRest.parents[child];
            while (walk < count && walk != slot && !bound(walk)) {
                walk = _sourceRest.parents[walk];
            }
            if (walk != slot) {
                continue;
            }
            haveDescendant = true;
            int targetWalk =
                joints[static_cast<std::size_t>(_map.GetJointIndex(static_cast<HumanJoint>(child)))]
                    .parent;
            std::size_t depth = 0;
            while (targetWalk >= 0 && static_cast<std::size_t>(targetWalk) < joints.size() &&
                   targetWalk != receiver && depth++ < joints.size()) {
                targetWalk = joints[static_cast<std::size_t>(targetWalk)].parent;
            }
            compatible = compatible && targetWalk == receiver;
        }
        if (haveDescendant && compatible) {
            _folds.push_back({slot, _sourceRest.parents[slot], ancestor, depths[slot]});
        }
    }
    // Children first: a run of missing joints composes in source-chain order,
    // independent of the vocabulary's order. Ties retain vocabulary order.
    std::stable_sort(_folds.begin(), _folds.end(), [](const Fold& a, const Fold& b) {
        return a.depth > b.depth;
    });
    // A child's motion must reach a bound ancestor, not stop in another
    // unbound joint excluded by an incompatible target branch.
    std::bitset<count> eligible;
    for (const Fold& fold : _folds) {
        eligible.set(fold.bone);
    }
    _folds.erase(std::remove_if(_folds.begin(),
                                _folds.end(),
                                [&](const Fold& fold) {
                                    for (std::size_t walk = fold.parent; walk != fold.ancestor;
                                         walk = _sourceRest.parents[walk]) {
                                        if (!eligible.test(walk)) {
                                            return true;
                                        }
                                    }
                                    return false;
                                }),
                 _folds.end());
}

RetargetedPose
PoseRetargeter::_RestPose() const
{
    RetargetedPose rest;
    const std::vector<SkeletonJoint>& joints = _skeleton.GetJoints();
    rest.rotations.reserve(joints.size());
    rest.translations.reserve(joints.size());
    for (const SkeletonJoint& joint : joints)
    {
        rest.rotations.push_back(joint.restRotation);
        rest.translations.push_back(joint.restTranslation);
    }
    return rest;
}

RetargetedPose
PoseRetargeter::Retarget(const openstrata::motion::MotionPose& pose, RetargetDiagnostics* diagnostics) const
{
    // Joints the clip does not drive stay at rest rather than collapsing to
    // identity, so a partial clip leaves the rest of the rig alone.
    RetargetedPose result = _RestPose();
    result.timestamp = pose.timestamp;
    const std::size_t jointCount = _skeleton.GetSize();

    std::optional<MotionPose> folding;
    if (!_folds.empty()) {
        folding.emplace(pose);
    }
    std::array<std::size_t, HumanJointCount> receivers;
    receivers.fill(SourceRestPose::kNoParent);
    for (const Fold& fold : _folds) {
        MotionPose& folded = *folding;
        if (!folded.validRotations.test(fold.bone)) {
            continue;
        }
        const pxr::GfQuatf rotation = folded.localRotations[fold.bone].GetNormalized();
        const pxr::GfQuatf rest = _sourceRest.localRotations[fold.bone].GetNormalized();
        receivers[fold.bone] = fold.ancestor;
        if (rotation != rest && rotation != pxr::GfQuatf(-rest.GetReal(), -rest.GetImaginary())) {
            const pxr::GfQuatf parent = folded.validRotations.test(fold.parent)
                                            ? folded.localRotations[fold.parent]
                                            : _sourceRest.localRotations[fold.parent];
            folded.localRotations[fold.parent] =
                (parent * rotation * rest.GetInverse()).GetNormalized();
            folded.validRotations.set(fold.parent);
        }
        folded.localRotations[fold.bone] = rest;
    }
    const MotionPose& rotations = folding ? *folding : pose;

    for (std::size_t slot = 0; slot < openstrata::motion::HumanJointCount; ++slot)
    {
        if (!rotations.validRotations.test(slot))
        {
            continue;
        }
        const auto bone = static_cast<openstrata::motion::HumanJoint>(slot);
        const int jointIndex = _map.GetJointIndex(bone);
        if (jointIndex < 0 || static_cast<std::size_t>(jointIndex) >= jointCount)
        {
            // Checked before the detail is built: a clip reports the same bone
            // on every sample, and only the first report is kept.
            if (pose.validRotations.test(slot) && diagnostics &&
                !diagnostics->Has(RetargetDiagnosticCode::UnboundDrivenBone,
                                  openstrata::motion::HumanJointName(bone)))
            {
                diagnostics->Report(MakeRetargetDiagnostic(
                    RetargetDiagnosticCode::UnboundDrivenBone, Describe(bone),
                    "the clip drives it and the target rig binds no joint for "
                    "it" + (receivers[slot] < HumanJointCount
                        ? "; its rest-relative rotation was folded into " +
                          Describe(static_cast<HumanJoint>(receivers[slot]))
                        : std::string())));
            }
            continue;
        }
        result.rotations[static_cast<std::size_t>(jointIndex)] =
            _correction.Apply(bone, rotations.localRotations[slot]);
    }

    // Root motion. The clip carries body translation on the hips (motion
    // contract, "Coordinates and time"), so the hips slot is the source
    // regardless of which joint finally receives it.
    const RootMotionOptions& rootOptions = _options.rootMotion;
    if (rootOptions.mode != RootMotionMode::Ignore)
    {
        const auto hipsSlot = static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips);
        const int hipsJoint = _map.GetJointIndex(openstrata::motion::HumanJoint::Hips);

        int receiver = hipsJoint;
        if (rootOptions.mode == RootMotionMode::RootJoint)
        {
            receiver = rootOptions.rootJointIndex;
            if (!IsValidRootJoint(rootOptions, jointCount))
            {
                if (diagnostics)
                {
                    ReportInvalidRootJoint(rootOptions, jointCount, diagnostics);
                }
                receiver = SkeletonDescriptor::kNoParent;
            }
        }

        if (receiver >= 0 && static_cast<std::size_t>(receiver) < jointCount)
        {
            pxr::GfVec3f sourceTranslation = _sourceRest.localTranslations[hipsSlot];
            bool haveSource = false;
            if (pose.root.hasPosition)
            {
                sourceTranslation = pose.root.worldPosition;
                haveSource = true;
            }
            if (haveSource)
            {
                const std::vector<SkeletonJoint>& joints = _skeleton.GetJoints();
                result.translations[static_cast<std::size_t>(receiver)] = ResolveRootTranslation(
                    rootOptions, sourceTranslation, _sourceRest.localTranslations[hipsSlot],
                    joints[static_cast<std::size_t>(receiver)].restTranslation);
            }
        }
        else if (rootOptions.mode == RootMotionMode::Hips && diagnostics &&
                 !diagnostics->Has(RetargetDiagnosticCode::MissingRequiredBone,
                                   openstrata::motion::HumanJointName(openstrata::motion::HumanJoint::Hips)))
        {
            // Under 'root' the hips are not where the root lands, so their
            // absence costs no root motion and the invalid index above is the
            // whole report. Under 'hips' the receiver is out of this rig either
            // because the map binds no hips or because it binds them to an
            // index the rig does not have -- a map built against another
            // skeleton -- and both drop the root. Checked before the detail is
            // built, since every sample of a clip lands here.
            diagnostics->Report(MakeRetargetDiagnostic(
                RetargetDiagnosticCode::MissingRequiredBone, Describe(openstrata::motion::HumanJoint::Hips),
                MissingRequiredDetail(openstrata::motion::HumanJoint::Hips, rootOptions)));
        }
    }

    return result;
}

RetargetDiagnostics
DiagnoseRig(const SkeletonDescriptor& skeleton, const RetargetMap& map, const RetargetOptions& options)
{
    RetargetDiagnostics diagnostics;
    const std::vector<SkeletonJoint>& joints = skeleton.GetJoints();

    // Missing *for this rig*: unbound, or bound to an index the rig does not
    // have. A map carries indices and never says which skeleton it counted
    // them against, so one built against another rig binds a bone the
    // retarget can only drop -- and `FindMissingRequiredBones`, which reads
    // the map alone, cannot see that.
    //
    // The caller's set, in its order. Under 'hips' the root lands on the hips,
    // so the mode requires them itself, and they are asked first when the set
    // does not name them: a caller with no required-bone rule still hears that
    // its root motion was dropped.
    std::vector<openstrata::motion::HumanJoint> required = options.requiredBones;
    if (options.rootMotion.mode == RootMotionMode::Hips &&
        std::find(required.begin(), required.end(), openstrata::motion::HumanJoint::Hips) ==
            required.end())
    {
        required.insert(required.begin(), openstrata::motion::HumanJoint::Hips);
    }
    for (const openstrata::motion::HumanJoint bone : required)
    {
        const int jointIndex = map.GetJointIndex(bone);
        if (jointIndex >= 0 && static_cast<std::size_t>(jointIndex) < joints.size())
        {
            continue;
        }
        diagnostics.Report(MakeRetargetDiagnostic(RetargetDiagnosticCode::MissingRequiredBone,
                                                  Describe(bone),
                                                  MissingRequiredDetail(bone, options.rootMotion)));
    }

    for (const int duplicate : map.FindDuplicateJointIndices())
    {
        // Named by the rig's own token where the index is one of its joints,
        // since a subject is what two implementations are compared on and an
        // index means nothing without the rig beside it.
        const bool inRig = duplicate >= 0 && static_cast<std::size_t>(duplicate) < joints.size();
        const std::string subject =
            inRig ? joints[static_cast<std::size_t>(duplicate)].token : std::to_string(duplicate);
        std::vector<openstrata::motion::HumanJoint> bound;
        for (std::size_t slot = 0; slot < openstrata::motion::HumanJointCount; ++slot)
        {
            const auto bone = static_cast<openstrata::motion::HumanJoint>(slot);
            if (map.GetJointIndex(bone) == duplicate)
            {
                bound.push_back(bone);
            }
        }
        std::string names;
        for (const openstrata::motion::HumanJoint bone : bound)
        {
            names += names.empty() ? "'" : ", '";
            names += Describe(bone);
            names += "'";
        }
        // The retarget writes bones in vocabulary order, so of the bones a
        // sample drives, the last one bound here is the one the joint keeps.
        diagnostics.Report(MakeRetargetDiagnostic(
            RetargetDiagnosticCode::DuplicateTarget, subject,
            names + " are bound to this joint; of those a sample drives, the "
                    "last in the vocabulary is the one it keeps"));
    }

    if (!skeleton.IsTopologicallyOrdered())
    {
        for (std::size_t i = 0; i < joints.size(); ++i)
        {
            const int parent = joints[i].parent;
            if (parent == SkeletonDescriptor::kNoParent ||
                (parent >= 0 && static_cast<std::size_t>(parent) < i))
            {
                continue;
            }
            diagnostics.Report(
                MakeRetargetDiagnostic(RetargetDiagnosticCode::InvalidHierarchy, joints[i].token,
                                       "this joint's parent index is " + std::to_string(parent) +
                                           ", which does not precede it; a skeleton states its "
                                           "joints in parent-before-child order"));
            break;
        }
    }

    if (options.rootMotion.mode == RootMotionMode::RootJoint &&
        !IsValidRootJoint(options.rootMotion, joints.size()))
    {
        ReportInvalidRootJoint(options.rootMotion, joints.size(), &diagnostics);
    }
    return diagnostics;
}

RetargetedAnimation
PoseRetargeter::Retarget(const openstrata::motion::MotionClip& animation,
                         RetargetDiagnostics* diagnostics) const
{
    RetargetedAnimation result;
    for (const SkeletonJoint& joint : _skeleton.GetJoints())
    {
        result.joints.push_back(joint.token);
    }
    result.startTime = animation.startTime;
    result.endTime = animation.endTime;
    result.frameRate = animation.nominalFrameRate;
    result.source = animation.source;

    if (diagnostics)
    {
        diagnostics->Merge(DiagnoseRig(_skeleton, _map, _options));
    }

    result.samples.reserve(animation.samples.size());
    // Every sample reports into one list, which keeps each bone once. Until
    // P1-1 only the first sample was asked, which kept each bone once as well
    // and missed any bone the clip started driving later.
    for (const openstrata::motion::MotionPose& pose : animation.samples)
    {
        result.samples.push_back(Retarget(pose, diagnostics));
    }

    if (!result.samples.empty())
    {
        result.startTime = result.samples.front().timestamp;
        result.endTime = result.samples.back().timestamp;
    }
    return result;
}

} // namespace openstrata::motion
