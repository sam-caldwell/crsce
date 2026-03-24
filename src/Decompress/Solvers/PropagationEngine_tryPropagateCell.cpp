/**
 * @file PropagationEngine_tryPropagateCell.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief PropagationEngine::tryPropagateCell -- fast-path propagation for single-cell assignment.
 */
#include "decompress/Solvers/PropagationEngine.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/LtpTable.h"

namespace crsce::decompress::solvers {
    /**
     * @name tryPropagateCell
     * @brief Fast-path propagation for a single-cell assignment.
     *
     * Computes the 4 basic flat stat indices plus LTP indices and checks
     * feasibility/forcing inline. Returns immediately if no forcing is needed.
     *
     * @param r Row index.
     * @param c Column index.
     * @return True if feasible; false if a contradiction was found.
     */
    auto PropagationEngine::tryPropagateCell(const std::uint16_t r, const std::uint16_t c) -> bool {
        forced_.clear();

        auto &cs = static_cast<ConstraintStore &>(store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // Compute 4 basic flat indices (same arithmetic as assign())
        const auto ri = static_cast<std::uint32_t>(r);
        const auto ci = static_cast<std::uint32_t>(kS) + c;
        const auto di = (2U * kS) + static_cast<std::uint32_t>(c - r + (kS - 1));
        const auto xi = (2U * kS) + ConstraintStore::kNumDiags + static_cast<std::uint32_t>(r + c);

        // B.57: LTP membership (2 sub-tables)
        const auto &mem = ltpMembership(r, c);

        // Check 4 basic + 2 LTP lines for feasibility and forcing
        const std::array<std::uint32_t, 6> indices = {  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            ri, ci, di, xi,
            static_cast<std::uint32_t>(mem.flat[0]), // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            static_cast<std::uint32_t>(mem.flat[1])  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        };
        bool needsForcing = false;

        for (const auto idx : indices) {
            const auto &stat = cs.getStatDirect(idx); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
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

        // Rare path: fall through to full propagation for forcing.
        const auto linesForCell = cs.getLinesForCell(r, c);
        return propagate(std::span<const LineID>{linesForCell.lines.data(),
                                                 static_cast<std::size_t>(linesForCell.count)});
    }
} // namespace crsce::decompress::solvers
