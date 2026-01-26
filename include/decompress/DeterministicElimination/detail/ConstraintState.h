/**
 * @file ConstraintState.h
 * @brief Residual constraints (R and U) for CRSCE v1 line families.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @name ConstraintState
     * @brief Residuals for rows, columns, diagonals, and anti-diagonals.
     */
    struct ConstraintState {
        /**
         * @name R_row
         * @brief Row residuals (remaining ones to place per row).
         */
        std::array<std::uint16_t, Csm::kS> R_row{};
        /**
         * @name R_col
         * @brief Column residuals (remaining ones to place per column).
         */
        std::array<std::uint16_t, Csm::kS> R_col{};
        /**
         * @name R_diag
         * @brief Diagonal residuals (remaining ones to place per diag).
         */
        std::array<std::uint16_t, Csm::kS> R_diag{};
        /**
         * @name R_xdiag
         * @brief Anti-diagonal residuals (remaining ones per xdiag).
         */
        std::array<std::uint16_t, Csm::kS> R_xdiag{};
        /**
         * @name U_row
         * @brief Row unknowns remaining (free cells per row).
         */
        std::array<std::uint16_t, Csm::kS> U_row{};
        /**
         * @name U_col
         * @brief Column unknowns remaining (free cells per column).
         */
        std::array<std::uint16_t, Csm::kS> U_col{};
        /**
         * @name U_diag
         * @brief Diagonal unknowns remaining (free cells per diag).
         */
        std::array<std::uint16_t, Csm::kS> U_diag{};
        /**
         * @name U_xdiag
         * @brief Anti-diagonal unknowns remaining (free cells per xdiag).
         */
        std::array<std::uint16_t, Csm::kS> U_xdiag{};
    };
} // namespace crsce::decompress
