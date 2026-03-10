/**
 * @file min_bytes_for_n_blocks.h
 * @brief Compute minimal bytes to span exactly N CRSCE blocks when packed row-major into 511x511 bits.
 * @author Sam Caldwell
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>

namespace crsce::testrunner::detail {
    /**
     * @name min_bytes_for_n_blocks
     * @brief Minimal input size that produces exactly n blocks (last block has 1 bit).
     * @param nblocks Number of CRSCE blocks desired (>= 1).
     * @return Minimal byte count to produce nblocks blocks after packing.
     */
    std::uint64_t min_bytes_for_n_blocks(std::uint64_t nblocks) noexcept;
}
