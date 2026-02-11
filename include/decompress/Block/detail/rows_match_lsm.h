/**
 * @file rows_match_lsm.h
 * @brief Check row-sum invariant against LSM.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <span>
#include "decompress/Csm/Csm.h"

namespace crsce::decompress::detail {
    /**
     * @name rows_match_lsm
     * @brief Verify each row’s 1-count equals its LSM target.
     * @param csm Cross‑Sum Matrix to check.
     * @param lsm Row target counts; size must be ≥ Csm::kS.
     * @return bool True if all rows match; false otherwise.
     */
    bool rows_match_lsm(const Csm &csm, std::span<const std::uint16_t> lsm);
}
