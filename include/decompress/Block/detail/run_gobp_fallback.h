/**
 * @file run_gobp_fallback.h
 * @brief GOBP fallback orchestration extracted from solve_block for testability and readability.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <span>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

// Run the GOBP fallback stages with adaptive restarts and finishers.
// Returns true if a solution was found and verified; false otherwise.
bool run_gobp_fallback(Csm &csm,
                       ConstraintState &st,
                       DeterministicElimination &det,
                       Csm &baseline_csm,
                       ConstraintState &baseline_st,
                       std::span<const std::uint8_t> lh,
                       std::span<const std::uint16_t> lsm,
                       std::span<const std::uint16_t> vsm,
                       std::span<const std::uint16_t> dsm,
                       std::span<const std::uint16_t> xsm,
                       BlockSolveSnapshot &snap,
                       std::uint64_t valid_bits);

} // namespace crsce::decompress::detail
