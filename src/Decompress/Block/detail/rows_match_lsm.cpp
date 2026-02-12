/**
 * @file rows_match_lsm.cpp
 * @brief Helper to check that each row’s bit count equals its LSM target.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/rows_match_lsm.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include "decompress/Csm/detail/Csm.h"

namespace crsce::decompress::detail {
    /**
     * @name rows_match_lsm
     * @brief Verify row-sum invariant against LSM targets.
     * @param csm Cross‑Sum Matrix to read.
     * @param lsm Row target counts; size must be ≥ Csm::kS.
     * @return bool True if every row’s 1-count matches; false otherwise.
     */
    bool rows_match_lsm(const Csm &csm, const std::span<const std::uint16_t> lsm) {
        const std::size_t S = Csm::kS;
        if (lsm.size() < S) { return false; }
        for (std::size_t r = 0; r < S; ++r) {
            std::size_t ones = 0;
            for (std::size_t c = 0; c < S; ++c) {
                if (csm.get(r, c)) { ++ones; }
            }
            if (ones != static_cast<std::size_t>(lsm[r])) { return false; }
        }
        return true;
    }
}
