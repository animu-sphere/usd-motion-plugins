// SPDX-License-Identifier: Apache-2.0
//
// What both halves of the standalone motion stage state about it
// (docs/design/USD_MAPPING.md §4.1, §8). Here rather than beside either half,
// because the reader checking a version the writer authored through the
// writer's header would make the reading half depend on the authoring one.
#pragma once

namespace openstrata::motion
{

// `customData.motion.contractVersion` on every stage this library authors,
// and the newest one it reads (USD_MAPPING.md §8).
inline constexpr int MotionStageContractVersion = 1;

// Always 30, whatever rate the samples were taken at (USD_MAPPING.md §4.1,
// USD-O2). The samples keep their own times; this is only where they are
// written. A stage authored elsewhere may state another rate, and the reader
// uses what it finds.
inline constexpr double MotionStageTimeCodesPerSecond = 30.0;

} // namespace openstrata::motion
