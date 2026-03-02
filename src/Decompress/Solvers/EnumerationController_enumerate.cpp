/**
 * @file EnumerationController_enumerate.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController::enumerate implementation (Algorithm 1).
 */
#include "decompress/Solvers/EnumerationController.h"

#include <cstdint>
#include <vector>

#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @name enumerate
     * @brief Enumerate feasible solutions in lexicographic order.
     * @param callback Called for each solution. Return true to continue, false to stop early.
     * @throws None
     */
    void EnumerationController::enumerate(const SolutionCallback &callback) {
        // Initial propagation: queue all lines
        std::vector<LineID> allLines;
        allLines.reserve(kS + kS + ((2 * kS) - 1) + ((2 * kS) - 1));
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::Row, .index = i});
        }
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::Column, .index = i});
        }
        for (std::uint16_t i = 0; i < (2 * kS) - 1; ++i) {
            allLines.push_back({.type = LineType::Diagonal, .index = i});
        }
        for (std::uint16_t i = 0; i < (2 * kS) - 1; ++i) {
            allLines.push_back({.type = LineType::AntiDiagonal, .index = i});
        }

        (*propagator_).reset();
        if (!propagator_->propagate(allLines)) {
            return; // initial state is infeasible
        }

        // Record initial forced assignments on the undo stack
        const auto &initialForced = propagator_->getForcedAssignments();
        for (const auto &a : initialForced) {
            brancher_->recordAssignment(a.r, a.c);
        }

        bool stop = false;
        dfs(callback, stop);
    }
} // namespace crsce::decompress::solvers
