/**
 * @file BlockSolver.h
 * @brief High-level block solver: reconstruct CSM from LH and cross-sum payloads.
 */
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @brief Solve a block given LH and sums; returns true on success and fills 'csm_out'.
     */
    bool solve_block(std::span<const std::uint8_t> lh,
                     std::span<const std::uint8_t> sums,
                     Csm &csm_out,
                     const std::string &seed);
}
