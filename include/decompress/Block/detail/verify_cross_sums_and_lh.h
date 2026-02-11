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
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    bool verify_cross_sums_and_lh(Csm &csm,
                                  const std::array<std::uint16_t, Csm::kS> &lsm,
                                  const std::array<std::uint16_t, Csm::kS> &vsm,
                                  const std::array<std::uint16_t, Csm::kS> &dsm,
                                  const std::array<std::uint16_t, Csm::kS> &xsm,
                                  std::span<const std::uint8_t> lh,
                                  BlockSolveSnapshot &snap);
}

