// SPDX-License-Identifier: Apache-2.0

#include "motionCore/BasisConversion.h"

#include <cmath>

namespace openstrata::motion
{

bool
IsValidBasis(const SignedPermutationBasis& basis) noexcept
{
    if (!std::isfinite(basis.scale) || basis.scale <= 0.0 ||
        (basis.determinant != 1 && basis.determinant != -1))
    {
        return false;
    }
    bool seen[3] = {false, false, false};
    int sign = 1;
    for (int i = 0; i < 3; ++i)
    {
        const int axis = basis.component[i];
        if (axis < 0 || axis > 2 || seen[axis])
        {
            return false;
        }
        seen[axis] = true;
        if (basis.negate[i])
        {
            sign = -sign;
        }
        for (int j = 0; j < i; ++j)
        {
            if (basis.component[j] > axis)
            {
                sign = -sign;
            }
        }
    }
    return sign == basis.determinant;
}

pxr::GfVec3f
ApplyBasisToPosition(const SignedPermutationBasis& basis, const pxr::GfVec3f& value) noexcept
{
    pxr::GfVec3f out(0.0f);
    for (int i = 0; i < 3; ++i)
    {
        const int source = basis.component[i];
        if (source < 0 || source > 2)
        {
            continue;
        }
        const double component = static_cast<double>(value[source]);
        out[i] = static_cast<float>((basis.negate[i] ? -component : component) * basis.scale);
    }
    return out;
}

pxr::GfQuatf
ApplyBasisToRotation(const SignedPermutationBasis& basis, const pxr::GfQuatf& value) noexcept
{
    const pxr::GfVec3f source = value.GetImaginary();
    double parts[4] = {0.0, 0.0, 0.0, static_cast<double>(value.GetReal())};
    for (int i = 0; i < 3; ++i)
    {
        const int axis = basis.component[i];
        if (axis < 0 || axis > 2)
        {
            continue;
        }
        const double component = static_cast<double>(source[axis]);
        parts[i] = (basis.negate[i] ? -component : component) * basis.determinant;
    }
    double lengthSquared = 0.0;
    for (const double part : parts)
    {
        lengthSquared += part * part;
    }
    const double length = std::sqrt(lengthSquared);
    if (!std::isfinite(length) || length <= 0.0)
    {
        return pxr::GfQuatf(static_cast<float>(parts[3]),
                            pxr::GfVec3f(static_cast<float>(parts[0]),
                                         static_cast<float>(parts[1]),
                                         static_cast<float>(parts[2])));
    }
    const double inverse = 1.0 / length;
    return pxr::GfQuatf(static_cast<float>(parts[3] * inverse),
                        pxr::GfVec3f(static_cast<float>(parts[0] * inverse),
                                     static_cast<float>(parts[1] * inverse),
                                     static_cast<float>(parts[2] * inverse)));
}

} // namespace openstrata::motion
