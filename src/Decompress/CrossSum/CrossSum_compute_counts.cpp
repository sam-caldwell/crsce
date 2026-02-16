/**
 * @file CrossSum_compute_counts.cpp
 * @brief Compute per-index ones counts for the selected CrossSum family.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name compute_counts
     * @brief Return live per-index ones counts for the current family from CSM.
     * @param csm Cross‑Sum Matrix providing counters.
     * @return Vec Per-index counts across the selected family.
     */
    CrossSum::Vec CrossSum::compute_counts(const Csm &csm) const {
        Vec out{};
        for (std::size_t i = 0; i < kS; ++i) {
            switch (fam_) {
                case CrossSumFamily::Row: out.at(i) = csm.count_lsm(i); break;
                case CrossSumFamily::Col: out.at(i) = csm.count_vsm(i); break;
                case CrossSumFamily::Diag: out.at(i) = csm.count_dsm(i); break;
                case CrossSumFamily::XDiag: out.at(i) = csm.count_xsm(i); break;
            }
        }
        return out;
    }
}
