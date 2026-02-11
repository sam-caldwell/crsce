/**
 * @file GobpSolver_indices.cpp
 * @brief Out-of-class definitions for GobpSolver constexpr index helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt
 */

#include "decompress/Phases/Gobp/GobpSolver.h"

namespace crsce::decompress {
    constexpr std::size_t GobpSolver::diag_index(const std::size_t r, const std::size_t c) noexcept {
        return (c >= r) ? (c - r) : (c + S - r);
    }
    constexpr std::size_t GobpSolver::xdiag_index(const std::size_t r, const std::size_t c) noexcept {
        return (r + c) % S;
    }
}

