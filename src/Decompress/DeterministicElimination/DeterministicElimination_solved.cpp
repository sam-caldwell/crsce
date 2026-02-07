/**
 * @file DeterministicElimination_solved.cpp
 * @brief Implementation of DeterministicElimination::solved.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::solved
     * @brief Whether the matrix is fully solved (no undecided cells remain).
     * @return bool True if solved; false otherwise.
     */
    bool DeterministicElimination::solved() const {
        for (std::size_t i = 0; i < S; ++i) {
            if (st_.U_row.at(i) != 0 || st_.U_col.at(i) != 0 || st_.U_diag.at(i) != 0 || st_.U_xdiag.at(i) != 0) {
                return false;
            }
        }
        return true;
    }
} // namespace crsce::decompress
