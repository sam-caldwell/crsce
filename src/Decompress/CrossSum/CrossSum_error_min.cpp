/**
 * @file CrossSum_error_min.cpp
 * @brief Minimum absolute error among indices for selected family.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"  // CrossSum
#include "decompress/Csm/Csm.h"  // Csm
#include <cstdint>
#include <algorithm>

namespace crsce::decompress {
    /**
     * @name error_min
     * @brief Return the minimum |target - count| across indices for the family.
     * @param csm Cross‑Sum Matrix providing live counts.
     * @return std::uint16_t Minimum absolute error across indices.
     */
    std::uint16_t CrossSum::error_min(const Csm &csm) const {
        const Vec d = compute_deficit(csm);
        std::uint16_t m = 0xFFFFU;
        for (auto v : d) { m = std::min<std::uint16_t>(v, m); }
        return (m == 0xFFFFU) ? 0U : m;
    }
}
