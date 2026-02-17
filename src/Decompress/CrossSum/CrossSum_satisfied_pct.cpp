/**
 * @file CrossSum_satisfied_pct.cpp
 * @brief Compute percentage of indices where live counts match targets.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSumType.h"  // CrossSum
#include "decompress/Csm/Csm.h"  // Csm
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name satisfied_pct
     * @brief Return 0..100 percentage of indices where count == target.
     * @param csm Cross‑Sum Matrix providing live counts.
     * @return int Percentage of satisfied indices.
     */
    int CrossSum::satisfied_pct(const Csm &csm) const {
        const Vec cnt = compute_counts(csm);
        std::size_t ok = 0;
        for (std::size_t i = 0; i < kS; ++i) {
            if (cnt.at(i) == tgt_.at(i)) { ++ok; }
        }
        return static_cast<int>((ok * 100ULL) / kS);
    }
}
