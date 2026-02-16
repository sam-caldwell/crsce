/**
 * @file solve_block.h
 * @brief High-level block solver: reconstruct CSM from LH and cross-sum payloads.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <span>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @name solve_block
     * @brief Solve a block given LH and sums; fills csm_out on success.
     * @param lh Span of LH payload bytes.
     * @param sums Span of cross-sum payload bytes.
     * @param csm_out Output CSM to populate when successful.
     * @param valid_bits Number of valid bits within the block; bits beyond this
     *        are considered padding and should be treated as zero-locked before solving.
     * @return true on success; false on failure.
     */
    bool solve_block(std::span<const std::uint8_t> lh,
                     std::span<const std::uint8_t> sums,
                     Csm &csm_out,
                     std::uint64_t valid_bits);
}
