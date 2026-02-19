/**
 * @file CrossSum_compute_deficit.cpp
 * @brief Compute absolute deficits between targets and live counts for a family.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"  // CrossSum
#include "decompress/Csm/Csm.h"  // Csm
#include <cstddef>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name compute_deficit
     * @brief Return |target - count| per index for the selected family.
     * @param csm Cross‑Sum Matrix providing live counts.
     * @return Vec Per-index absolute deficits.
     */
    CrossSum::Vec CrossSum::compute_deficit(const Csm &csm) const {
        const Vec cnt = compute_counts(csm);
        Vec out{};
        for (std::size_t i = 0; i < kS; ++i) {
            const auto t = tgt_.at(i);
            const auto c = cnt.at(i);
            out.at(i) = (c > t) ? static_cast<std::uint16_t>(c - t)
                                : static_cast<std::uint16_t>(t - c);
        }
        return out;
    }
}
