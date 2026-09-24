// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionCore/api.h"

#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec3f.h"

#include <array>

namespace openstrata::motion
{

// Canonical component i reads component[i] of the source, optionally negated.
// determinant must describe that signed permutation; scale is metres per
// source unit and applies only to positions. Producers construct this value
// from their own profile or measured source convention.
struct SignedPermutationBasis
{
    std::array<int, 3> component = {0, 1, 2};
    std::array<bool, 3> negate = {false, false, false};
    int determinant = 1;
    double scale = 1.0;

    friend bool operator==(const SignedPermutationBasis& lhs,
                           const SignedPermutationBasis& rhs) noexcept
    {
        return lhs.component == rhs.component && lhs.negate == rhs.negate &&
               lhs.determinant == rhs.determinant && lhs.scale == rhs.scale;
    }
    friend bool operator!=(const SignedPermutationBasis& lhs,
                           const SignedPermutationBasis& rhs) noexcept
    {
        return !(lhs == rhs);
    }
};

// Check untrusted profile data before using the conversion. Rotations must be
// finite and nonzero at the source boundary; these functions do not repair
// malformed samples.
MOTIONCORE_API bool IsValidBasis(const SignedPermutationBasis& basis) noexcept;
MOTIONCORE_API pxr::GfVec3f ApplyBasisToPosition(const SignedPermutationBasis& basis,
                                                const pxr::GfVec3f& value) noexcept;
// (w, det(M) * M v), normalized in double precision before narrowing to float.
MOTIONCORE_API pxr::GfQuatf ApplyBasisToRotation(const SignedPermutationBasis& basis,
                                                const pxr::GfQuatf& value) noexcept;

} // namespace openstrata::motion
