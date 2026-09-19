// SPDX-License-Identifier: Apache-2.0
//
// Stable, vendor-neutral motion values. This is intentionally a value-type
// contract: no USD stage, plug, file-format, network, or vendor SDK API is
// allowed here.
#pragma once

#include "motionCore/api.h"

#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec3f.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openstrata::motion
{

// The joint vocabulary, version 1: the 55-joint humanoid
// (docs/design/MOTION_CONTRACT.md §2). Values are stable array indices; a joint
// is appended only before Count, with a contract version bump.
//
// The list and its hierarchy follow the VRM 1.0 humanoid, which is where every
// producer this arrived with already mapped onto. That is its origin and not its
// meaning: the vocabulary carries no required-joint rule and no format binding.
enum class HumanJoint : std::uint8_t
{
    Hips,
    Spine,
    Chest,
    UpperChest,
    Neck,
    Head,
    LeftEye,
    RightEye,
    Jaw,

    LeftUpperLeg,
    LeftLowerLeg,
    LeftFoot,
    LeftToes,
    RightUpperLeg,
    RightLowerLeg,
    RightFoot,
    RightToes,

    LeftShoulder,
    LeftUpperArm,
    LeftLowerArm,
    LeftHand,
    RightShoulder,
    RightUpperArm,
    RightLowerArm,
    RightHand,

    LeftThumbMetacarpal,
    LeftThumbProximal,
    LeftThumbDistal,
    LeftIndexProximal,
    LeftIndexIntermediate,
    LeftIndexDistal,
    LeftMiddleProximal,
    LeftMiddleIntermediate,
    LeftMiddleDistal,
    LeftRingProximal,
    LeftRingIntermediate,
    LeftRingDistal,
    LeftLittleProximal,
    LeftLittleIntermediate,
    LeftLittleDistal,

    RightThumbMetacarpal,
    RightThumbProximal,
    RightThumbDistal,
    RightIndexProximal,
    RightIndexIntermediate,
    RightIndexDistal,
    RightMiddleProximal,
    RightMiddleIntermediate,
    RightMiddleDistal,
    RightRingProximal,
    RightRingIntermediate,
    RightRingDistal,
    RightLittleProximal,
    RightLittleIntermediate,
    RightLittleDistal,

    Count,
};

inline constexpr std::size_t HumanJointCount = static_cast<std::size_t>(HumanJoint::Count);

MOTIONCORE_API bool IsValidHumanJoint(HumanJoint joint) noexcept;
MOTIONCORE_API std::string_view HumanJointName(HumanJoint joint) noexcept;
MOTIONCORE_API std::optional<HumanJoint> FindHumanJoint(std::string_view name) noexcept;

// The canonical joint hierarchy. Nullopt for Hips, which is the root, and for
// Count.
//
// This lives here rather than in each consumer because a second copy of the
// taxonomy is a defect waiting to happen: a file reader and a live-capture path
// author the same semantic skeleton, and two tables that can disagree would
// produce two skeletons that look alike and do not compose.
MOTIONCORE_API std::optional<HumanJoint> HumanJointParent(HumanJoint joint) noexcept;

// The nearest ancestor of `joint` that `present` carries, skipping joints the
// rig does not solve -- a capture rig with no `upperChest` still parents its
// shoulders somewhere. Nullopt when no ancestor is present.
MOTIONCORE_API std::optional<HumanJoint>
NearestPresentAncestor(HumanJoint joint, const std::bitset<HumanJointCount>& present) noexcept;

// The semantic joint path for `joint` within a rig carrying `present`, e.g.
// "hips/spine/chest/neck/head". This is the token a `UsdSkelSkeleton` built
// from humanoid semantics carries; the string is plain text, and authoring it
// onto a stage stays with the consumer.
MOTIONCORE_API std::string HumanJointPath(HumanJoint joint,
                                              const std::bitset<HumanJointCount>& present);

enum class MotionSourceKind : std::uint8_t
{
    Clip,
    LiveCapture,
    Generated,
    Procedural,
    Simulated,
};

// Where a sample came from (docs/design/MOTION_CONTRACT.md §7). Recorded and
// reported, and never a branch condition: a consumer that cannot tell a
// tracker-driven pose from a clip-driven one is reading the value correctly.
//
// The first four fields name the source and are the same for every sample it
// produced. The last two are the sample's own, and both are optional because
// not every producer has them: a file has no sequence, and a sender that stamps
// nothing has no clock of its own to report.
struct SourceMetadata
{
    MotionSourceKind kind = MotionSourceKind::Clip;
    std::string provider;
    std::string protocol;
    std::string sourceId;

    // The producer's own stamp on this sample, in seconds, exactly as it
    // arrived -- its clock, its epoch. `MotionPose::timestamp` is the time the
    // motion layer samples on; this is the evidence the two can be compared
    // against, and nothing here converts one into the other.
    std::optional<double> sourceTimestamp;

    // The producer's counter for this sample. A gap is a sample that never
    // arrived, which is how a drop is told apart from a slow sender.
    std::optional<std::uint64_t> sequenceNumber;
};

// Exact value equality, on the aggregates that cross a boundary: the two
// OpenExec asks for it, a trace round-trip is defined by it, and a test that
// wants to know whether two poses describe the same *motion* wants
// `NearlyEqual` from Compare.h instead. That header carries the reasoning for
// both, including why a pose does not compare the fields it says it does not
// carry. The declarative `MotionConstraintSet` types are deliberately left
// without one: nothing compares them yet, and an operator no caller exercises
// is untested surface.
MOTIONCORE_API bool operator==(const SourceMetadata& a,
                               const SourceMetadata& b) noexcept;
MOTIONCORE_API bool operator!=(const SourceMetadata& a,
                               const SourceMetadata& b) noexcept;

// All positions and orientations are expressed independently of the hips local
// transform. Booleans distinguish a missing root sample from a zero-valued one.
struct RootMotion
{
    pxr::GfVec3f worldPosition = pxr::GfVec3f(0.0f);
    pxr::GfQuatf worldOrientation = pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f));
    pxr::GfVec3f linearVelocity = pxr::GfVec3f(0.0f);
    pxr::GfVec3f angularVelocity = pxr::GfVec3f(0.0f);

    bool hasPosition = false;
    bool hasOrientation = false;
    bool hasLinearVelocity = false;
    bool hasAngularVelocity = false;
};

MOTIONCORE_API bool operator==(const RootMotion& a, const RootMotion& b) noexcept;
MOTIONCORE_API bool operator!=(const RootMotion& a, const RootMotion& b) noexcept;

enum class FootContact : std::uint8_t
{
    Unknown,
    NotInContact,
    InContact,
};

struct ContactState
{
    FootContact leftFoot = FootContact::Unknown;
    FootContact rightFoot = FootContact::Unknown;
};

MOTIONCORE_API bool operator==(const ContactState& a, const ContactState& b) noexcept;
MOTIONCORE_API bool operator!=(const ContactState& a, const ContactState& b) noexcept;

// One channel value, under the name its producer used
// (docs/design/MOTION_CONTRACT.md §6).
//
// The joint vocabulary is closed, so it is an enum. The channel vocabulary is
// not: an avatar format defines preset expression names and then lets an author
// add their own, and a live sender's blend-shape names are whatever its model
// carries. So a name is carried verbatim. Deciding that one producer's "Joy" is
// a particular rig's `happy` is a resolve step in the consumer -- it needs the
// rig, which this layer does not have, and a table here would be a guess
// applied to every producer at once.
//
// The value is a scalar, because every channel measured so far is a weight.
// Whether a later non-scalar channel makes it a closed variant or a `VtValue`
// is MC-O4, decided when that channel arrives and not before.
struct MotionChannel
{
    std::string name;

    // A weight is conventionally in [0, 1] and deliberately not clamped: a
    // sender that said 1.5 said 1.5, and a value type that quietly corrected it
    // would hide the sender from the operator judging the session.
    float value = 0.0f;
};

MOTIONCORE_API bool operator==(const MotionChannel& a, const MotionChannel& b) noexcept;
MOTIONCORE_API bool operator!=(const MotionChannel& a, const MotionChannel& b) noexcept;

// The channel values one sample reported.
//
// Sorted by name, each name once -- an invariant rather than a convention, and
// `Set` is what maintains it. Two producers that reported the same values in a
// different order are the same motion, so they must be the same value; and a
// trace written from either has to round-trip to the same bytes. Neither holds
// for a list in arrival order, and both are load-bearing here.
//
// An absent name is not a zero value. A name this set does not carry was not
// reported, exactly as a joint outside `validRotations` was not reported. `Find`
// answers with a pointer rather than a value so the two cannot be confused.
struct MotionChannelSet
{
    std::vector<MotionChannel> entries;

    // Sets `name` to `value`, keeping `entries` sorted. Returns false when
    // `name` was already present: the new value replaces the old one, and the
    // caller is told, because only the caller knows whether a repeat is a
    // duplicate delivery to count or an update to accept.
    MOTIONCORE_API bool Set(std::string_view name, float value);

    // The value reported for `name`, or null when it was not reported.
    MOTIONCORE_API const float* Find(std::string_view name) const noexcept;

    bool
    IsEmpty() const noexcept
    {
        return entries.empty();
    }
};

MOTIONCORE_API bool operator==(const MotionChannelSet& a, const MotionChannelSet& b) noexcept;
MOTIONCORE_API bool operator!=(const MotionChannelSet& a, const MotionChannelSet& b) noexcept;

struct MotionPose
{
    MOTIONCORE_API MotionPose();

    // Seconds, never integer frame numbers.
    double timestamp = 0.0;
    RootMotion root;

    // Rotations are local to the semantic parent joint. `validRotations`
    // allows sparse capture data and clips that intentionally omit a joint.
    std::array<pxr::GfQuatf, HumanJointCount> localRotations;
    std::bitset<HumanJointCount> validRotations;

    std::optional<std::array<float, HumanJointCount>> confidence;
    std::optional<ContactState> contacts;

    // What this sample said beyond its joints -- so far, the character's face --
    // on the same timeline as its body. Empty means it reported none, and there is no
    // separate absent state because there is nothing it would mean that "the
    // producer reported none" does not -- an `optional` here would make two
    // values differ over a distinction neither carries.
    //
    // This is on the pose rather than in a track of its own, and that was the
    // decision (docs/design/MOTION_CONTRACT.md §6). The producers measured put
    // expressions on the pose's instants already: a live pose sender carries
    // joints and blend values in one message, and an animation-file reader
    // evaluates every channel at the union of their key times. A parallel track
    // would have needed a second buffer, a second intake policy and a second
    // resampler to carry data that arrives at the same instants anyway.
    MotionChannelSet channels;

    // Where this sample says the character is looking: a target *point*, in the
    // same space as `root.worldPosition`, and never a direction.
    //
    // A direction is only meaningful next to a head, and which head -- where it
    // sits, how far the eyes are from it -- is a property of a rig this layer
    // does not have. So the point the producer named is carried, and a format
    // repository turns it into eye rotations or expression weights against one
    // avatar's own look-at configuration. This is the same division the
    // channels above are under, for the same reason.
    //
    // It is a pose field and not a channel because a channel's value is a
    // scalar (MC-O4). Moving gaze into the channel set is that decision's, and
    // the import deliberately did not take it (usd-vrm-plugins' WORKSPACE.md
    // §9.5, finding 2).
    //
    // The offset from the head joint that the *source* rig measured is a
    // constant of that rig rather than of a sample, so it travels beside the
    // clip and not here.
    //
    // Optional rather than a sentinel, because the origin is a point a producer
    // can legitimately look at: "reported no target" cannot be spelled as a
    // value of the target.
    std::optional<pxr::GfVec3f> lookAtTarget;

    // Where this sample came from (MOTION_CONTRACT.md §5.1, §7). Always
    // present: a sample always came from somewhere, and a default value --
    // kind `Clip`, every string empty, no stamp and no sequence -- is how a
    // producer says it recorded nothing about where. An `optional` here gave
    // "unknown" two spellings, an empty metadata and no metadata, and two
    // samples of one motion could differ over which one a producer happened
    // to use.
    SourceMetadata metadata;
};

MOTIONCORE_API bool operator==(const MotionPose& a, const MotionPose& b) noexcept;
MOTIONCORE_API bool operator!=(const MotionPose& a, const MotionPose& b) noexcept;

struct MotionClip
{
    std::vector<MotionPose> samples;
    double startTime = 0.0;
    double endTime = 0.0;
    double nominalFrameRate = 30.0;
    SourceMetadata source;
};

MOTIONCORE_API bool operator==(const MotionClip& a, const MotionClip& b) noexcept;
MOTIONCORE_API bool operator!=(const MotionClip& a, const MotionClip& b) noexcept;

// Coordinate-space identifiers are values, not USD schema names. Stage
// authoring and conversion live in the consuming file-format/retarget layers.
enum class CoordinateSpace : std::uint8_t
{
    World,
    Character,
    Skeleton,
    JointLocal,
};

struct ConstraintCommon
{
    double targetTime = 0.0;
    std::optional<HumanJoint> joint;
    CoordinateSpace coordinateSpace = CoordinateSpace::Character;
    float weight = 1.0f;
    bool hard = false;
    std::optional<double> validFrom;
    std::optional<double> validUntil;
    SourceMetadata source;
};

struct RootWaypoint
{
    ConstraintCommon common;
    pxr::GfVec3f position = pxr::GfVec3f(0.0f);
};

struct RootTrajectorySample
{
    ConstraintCommon common;
    pxr::GfVec3f position = pxr::GfVec3f(0.0f);
    std::optional<pxr::GfQuatf> orientation;
};

struct FullBodyKeyframe
{
    ConstraintCommon common;
    MotionPose pose;
};

struct JointPositionConstraint
{
    ConstraintCommon common;
    pxr::GfVec3f position = pxr::GfVec3f(0.0f);
};

struct JointRotationConstraint
{
    ConstraintCommon common;
    pxr::GfQuatf rotation = pxr::GfQuatf(1.0f, pxr::GfVec3f(0.0f));
};

struct MotionConstraintSet
{
    std::vector<RootWaypoint> rootWaypoints;
    std::vector<RootTrajectorySample> rootTrajectory;
    std::vector<FullBodyKeyframe> keyframes;
    std::vector<JointPositionConstraint> jointPositions;
    std::vector<JointRotationConstraint> jointRotations;
    std::optional<std::string> textPrompt;
};

} // namespace openstrata::motion
