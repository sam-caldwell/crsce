/**
 * @file CrossSum_error_max.cpp
 * @brief Maximum absolute error among indices for selected family.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSumType.h"  // CrossSum
#include "decompress/Csm/Csm.h"  // Csm
#include <cstdint>
#include <algorithm>

namespace crsce::decompress {
    /**
     * @name error_max
     * @brief Return the maximum |target - count| across indices for the family.
     * @param csm Cross‑Sum Matrix providing live counts.
     * @return std::uint16_t Maximum absolute error across indices.
     */
    std::uint16_t CrossSum::error_max(const Csm &csm) const {
        const Vec d = compute_deficit(csm);
        std::uint16_t m = 0U;
        for (auto v : d) { m = std::max<std::uint16_t>(v, m); }
        return m;
    }
}
