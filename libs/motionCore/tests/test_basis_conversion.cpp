// SPDX-License-Identifier: Apache-2.0

#include "motionCore/BasisConversion.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace
{

bool Near(float a, float b) { return std::abs(a - b) < 1.0e-5f; }

bool Near(const pxr::GfVec3f& a, const pxr::GfVec3f& b)
{
    return Near(a[0], b[0]) && Near(a[1], b[1]) && Near(a[2], b[2]);
}

} // namespace

int main()
{
    using namespace openstrata::motion;
    const SignedPermutationBasis identity;
    assert(IsValidBasis(identity));
    assert(Near(ApplyBasisToPosition(identity, pxr::GfVec3f(1, 2, 3)),
                pxr::GfVec3f(1, 2, 3)));

    SignedPermutationBasis mirror;
    mirror.negate[0] = true;
    mirror.determinant = -1;
    assert(IsValidBasis(mirror));
    assert(Near(ApplyBasisToPosition(mirror, pxr::GfVec3f(1, 2, 3)),
                pxr::GfVec3f(-1, 2, 3)));
    const float half = std::sqrt(0.5f);
    const pxr::GfQuatf sourceYaw(half, pxr::GfVec3f(0, half, 0));
    const pxr::GfQuatf canonicalYaw = ApplyBasisToRotation(mirror, sourceYaw);
    // A positive source yaw mirrored through X turns canonical +Z toward -X.
    assert(Near(canonicalYaw.Transform(pxr::GfVec3f(0, 0, 1)),
                pxr::GfVec3f(-1, 0, 0)));

    SignedPermutationBasis turned;
    turned.component = {2, 1, 0};
    turned.negate = {false, false, true};
    turned.scale = 0.01;
    assert(IsValidBasis(turned));
    assert(Near(ApplyBasisToPosition(turned, pxr::GfVec3f(100, 200, 300)),
                pxr::GfVec3f(3, 2, -1)));

    const float tiny = std::numeric_limits<float>::denorm_min();
    const pxr::GfQuatf normalized = ApplyBasisToRotation(
        mirror, pxr::GfQuatf(tiny, pxr::GfVec3f(0, tiny, 0)));
    assert(Near(normalized.GetReal(), half));
    assert(Near(normalized.GetImaginary(), pxr::GfVec3f(0, -half, 0)));

    turned.component = {0, 0, 2};
    assert(!IsValidBasis(turned));
    turned = identity;
    turned.determinant = -1;
    assert(!IsValidBasis(turned));
    turned = identity;
    turned.scale = 0.0;
    assert(!IsValidBasis(turned));
}
