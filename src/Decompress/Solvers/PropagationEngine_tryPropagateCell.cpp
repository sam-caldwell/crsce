/**
 * @file PropagationEngine_tryPropagateCell.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief PropagationEngine::tryPropagateCell -- fast-path propagation for single-cell assignment.
 */
#include "decompress/Solvers/PropagationEngine.h"

#include <array>
#include <cstdint>

#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @name tryPropagateCell
     * @brief Fast-path propagation for a single-cell assignment.
     *
     * Computes the 4 flat stats indices directly and checks feasibility/forcing inline.
     * Returns immediately if no forcing is needed (the common case ~80% of iterations).
     * Falls back to full propagate() only when forcing is required.
     *
     * @param r Row index.
     * @param c Column index.
     * @return True if feasible; false if a contradiction was found.
     */
    auto PropagationEngine::tryPropagateCell(const std::uint16_t r, const std::uint16_t c) -> bool {
        forced_.clear();

        auto &cs = static_cast<ConstraintStore &>(store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // Compute 4 flat indices (same arithmetic as assign())
        const auto ri = static_cast<std::uint32_t>(r);
        const auto ci = static_cast<std::uint32_t>(kS) + c;
        const auto di = (2U * kS) + static_cast<std::uint32_t>(c - r + (kS - 1));
        const auto xi = (2U * kS) + ConstraintStore::kNumDiags + static_cast<std::uint32_t>(r + c);

        // Check all 4 lines for feasibility and forcing
        const std::array<std::uint32_t, 4> indices = {ri, ci, di, xi};
        bool needsForcing = false;

        for (const auto idx : indices) {
            const auto &stat = cs.getStatDirect(idx);
            const auto rho = static_cast<std::int32_t>(stat.target) - static_cast<std::int32_t>(stat.assigned);
            const auto u = static_cast<std::int32_t>(stat.unknown);
            if (rho < 0 || rho > u) {
                return false; // infeasible
            }
            if (u > 0 && (rho == 0 || rho == u)) {
                needsForcing = true;
            }
        }

        if (!needsForcing) {
            return true; // FAST PATH: ~80% of iterations exit here
        }

        // Rare path: fall through to full propagation for forcing
        const std::array<LineID, 4> lines = cs.getLinesForCell(r, c);
        return propagate(lines);
    }
} // namespace crsce::decompress::solvers
