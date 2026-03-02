/**
 * @file ConstraintStore_getResidual.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getResidual implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstdint>

#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @name getResidual
     * @brief Get the residual rho(L) = S(L) - a(L) for a line.
     * @param line The line identifier.
     * @return The residual count (can be negative if over-assigned).
     * @throws None
     */
    auto ConstraintStore::getResidual(const LineID line) const -> std::int32_t {
        const auto &stat = stats_[lineIndex(line)]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        return static_cast<std::int32_t>(stat.target) - static_cast<std::int32_t>(stat.assigned);
    }
} // namespace crsce::decompress::solvers
