// SPDX-License-Identifier: Apache-2.0
#include "motionSource/SourceProvenance.h"

namespace openstrata::motion
{

bool
operator==(const SourceProvenance& lhs, const SourceProvenance& rhs) noexcept
{
    return lhs.producer == rhs.producer && lhs.producerVersion == rhs.producerVersion &&
           lhs.profileId == rhs.profileId && lhs.format == rhs.format &&
           lhs.sourceId == rhs.sourceId;
}

bool
operator!=(const SourceProvenance& lhs, const SourceProvenance& rhs) noexcept
{
    return !(lhs == rhs);
}

} // namespace openstrata::motion
