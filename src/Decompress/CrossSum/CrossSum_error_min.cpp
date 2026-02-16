/**
 * @file CrossSum_error_min.cpp
 */
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Csm/Csm.h"
#include <cstdint>
#include <algorithm>

namespace crsce::decompress {
    std::uint16_t CrossSum::error_min(const Csm &csm) const {
        const Vec d = compute_deficit(csm);
        std::uint16_t m = 0xFFFFU;
        for (auto v : d) { m = std::min<std::uint16_t>(v, m); }
        return (m == 0xFFFFU) ? 0U : m;
    }
}
