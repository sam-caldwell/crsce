/**
 * @file verify_cross_sums_and_lh.h
 * @brief Verify cross-sums and LH signatures for a solved block.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSums.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @name verify_cross_sums_and_lh
     * @brief Validate cross-sum vectors and row-hash chain for a candidate solution.
     * @param csm Cross‑Sum Matrix to verify.
     * @param lsm Row targets.
     * @param vsm Column targets.
     * @param dsm Diagonal targets.
     * @param xsm Anti-diagonal targets.
     * @param lh LH payload bytes (per-row digests).
     * @param snap Snapshot to append timing.
     * @return bool True if both checks pass; false otherwise.
     */
    bool verify_cross_sums_and_lh(Csm &csm,
                                  const CrossSums &sums,
                                  std::span<const std::uint8_t> lh,
                                  BlockSolveSnapshot &snap);
}
