/**
 * @file CrossSum_error_max.cpp
 */
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Csm/Csm.h"
#include <cstdint>
#include <algorithm>

namespace crsce::decompress {
    std::uint16_t CrossSum::error_max(const Csm &csm) const {
        const Vec d = compute_deficit(csm);
        std::uint16_t m = 0U;
        for (auto v : d) { m = std::max<std::uint16_t>(v, m); }
        return m;
    }
}
