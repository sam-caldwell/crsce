/**
 * @file Detail_verify_row_sums.cpp
 * @brief verify_row_sums for Radditz helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <span>
#include <cstdint>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/Phases/RadditzSift/verify_row_sums.h"

namespace crsce::decompress::phases::detail {
    /**
     * @name verify_row_sums
     * @brief Validate that each row’s 1-count equals its LSM target.
     * @param csm Matrix to check.
     * @param lsm Row target counts (size ≥ Csm::kS).
     * @return bool True if all rows match; false otherwise.
     */
    bool verify_row_sums(const Csm &csm, const std::span<const std::uint16_t> lsm) {
        const std::size_t S = Csm::kS;
        if (lsm.size() < S) { return false; }
        for (std::size_t r = 0; r < S; ++r) {
            if (static_cast<std::size_t>(csm.count_lsm(r)) != static_cast<std::size_t>(lsm[r])) { return false; }
        }
        return true;
    }
}
