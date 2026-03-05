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
     * Computes the 4 basic flat stat indices plus 1-2 LTP indices (B.21) and checks
     * feasibility/forcing inline. Returns immediately if no forcing is needed (the
     * common case ~80% of iterations). Falls back to full propagate() when forcing required.
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

        // B.21: LTP membership is 1 or 2 sub-tables
        const auto &mem = ltpMembership(r, c);

        // Check all lines (4 basic + 1-2 LTP) for feasibility and forcing
        // Use a fixed 6-element array; only first (4 + mem.count) are valid
        const std::array<std::uint32_t, 6> indices = {  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            ri, ci, di, xi,
            static_cast<std::uint32_t>(mem.flat[0]),
            static_cast<std::uint32_t>(mem.count > 1 ? mem.flat[1] : mem.flat[0])
        };
        const auto numLines = static_cast<std::size_t>(4U + mem.count);
        bool needsForcing = false;

        for (std::size_t n = 0; n < numLines; ++n) {
            const auto &stat = cs.getStatDirect(indices[n]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
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
        // Pass all active lines so LTP lines cascade too.
        const auto linesForCell = cs.getLinesForCell(r, c);
        return propagate(std::span<const LineID>{linesForCell.lines.data(),
                                                 static_cast<std::size_t>(linesForCell.count)});
    }
} // namespace crsce::decompress::solvers
