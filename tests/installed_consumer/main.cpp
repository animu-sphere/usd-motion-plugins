// SPDX-License-Identifier: Apache-2.0
//
// Compiled against every installed package's public header (includes.h, which
// CMakeLists.txt generates from packages.json) and linked against every
// exported target. It prints how many it consumed, which the lane compares
// with packages.json, so a package that was silently skipped cannot pass.
#include "includes.h"
#include "motionUsd/SkeletonAnimationWriter.h"

#include <cstdio>

int
main()
{
    // Exercise the new installed header and symbol, including its null-stage
    // refusal, so merely adding a header without shipping the implementation
    // cannot pass the installed-consumer gate.
    if (openstrata::motion::AuthorSkeletonAnimation({}, {}, {}, {}, {}, nullptr)) {
        return 1;
    }
    std::printf("consumed %d package(s)\n", USDMOTION_CONSUMER_PACKAGES);
    return 0;
}
