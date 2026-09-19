# motion_bvh_inspect

Reports what a BVH file contains, in the file's own words: joints, depth,
channels, frames and frame time. It reports no unit, axis, handedness or
humanoid bone, because a BVH file states none of them. Those facts live in a
producer profile, which [`motion_convert`](../motionConvert/README.md) applies.

Both commands are documented in [the converter's README](../motionConvert/README.md#motion_bvh_inspect),
because the line between them is the point of each.
