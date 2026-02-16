/**
 * @file seed_initial_beliefs.h
 * @brief Seed initial beliefs into the CSM data layer.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress::detail {
    /**
     * @name seed_initial_beliefs
     * @brief Write initial probability guesses to the CSM data layer using residual pressure.
     * @param csm Cross‑Sum Matrix to update.
     * @param st Current constraint state guiding the heuristic.
     * @return std::uint64_t RNG seed used for reproducibility.
     */
    std::uint64_t seed_initial_beliefs(Csm &csm, const ConstraintState &st);
}
