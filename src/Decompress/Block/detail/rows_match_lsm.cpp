/**
 * @file rows_match_lsm.cpp
 * @brief Helper to check that each row’s bit count equals its LSM target.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/rows_match_lsm.h"
#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/LateralSumMatrix.h"

namespace crsce::decompress::detail {
    /**
     * @name rows_match_lsm
     * @brief Verify row-sum invariant against LSM targets.
     * @param csm Cross‑Sum Matrix to read.
     * @param lsm Row target counts; size must be ≥ Csm::kS.
     * @return bool True if every row’s 1-count matches; false otherwise.
     */
    bool rows_match_lsm(const Csm &csm, const ::crsce::decompress::xsum::LateralSumMatrix &lsm) {
        const std::size_t S = Csm::kS;
        if (lsm.size() < S) { return false; }
        for (std::size_t r = 0; r < S; ++r) {
            if (static_cast<std::size_t>(csm.count_lsm(r)) != static_cast<std::size_t>(lsm.targets()[r])) { return false; }
        }
        return true;
    }
}
