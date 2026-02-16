/**
 * @file ConstraintState.h
 * @brief Aggregator for residual constraints (R and U) declarations.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @name ConstraintState
     * @brief Residuals for rows, columns, diagonals, and anti-diagonals.
     */
    struct ConstraintState {
        /**
         * @name ConstraintState
         * @brief constructor
         * @param S CRSCE block size
         * @param lsm lateral sums matrix
         * @param vsm vertical sums matrix
         * @param dsm diagonal sums matrix
         * @param xsm anti-diagonal sums matrix
         */
        ConstraintState() = default;

        ConstraintState(
            const std::size_t S,
            const std::array<uint16_t, 511> &lsm,
            const std::array<uint16_t, 511> &vsm,
            const std::array<uint16_t, 511> &dsm,
            const std::array<uint16_t, 511> &xsm)
            : R_row(lsm), R_col(vsm), R_diag(dsm), R_xdiag(xsm) {
            const auto s16 = static_cast<std::uint16_t>(S);
            U_row.fill(s16);
            U_col.fill(s16);
            U_diag.fill(s16);
            U_xdiag.fill(s16);
        };
        /**
         * @name R_row
         * @brief Row residuals (remaining ones to place per row).
         * @return std::array<std::uint16_t, Csm::kS>
         */
        std::array<std::uint16_t, Csm::kS> R_row{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @name R_col
         * @brief Column residuals (remaining ones to place per column).
         */
        std::array<std::uint16_t, Csm::kS> R_col{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @name R_diag
         * @brief Diagonal residuals (DSM): d = (c − r) mod S.
         */
        std::array<std::uint16_t, Csm::kS> R_diag{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @name R_xdiag
         * @brief Anti-diagonal residuals (XSM): x = (r + c) mod S.
         */
        std::array<std::uint16_t, Csm::kS> R_xdiag{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @name U_row
         * @brief Row unknowns remaining (free cells per row).
         */
        std::array<std::uint16_t, Csm::kS> U_row{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @name U_col
         * @brief Column unknowns remaining (free cells per column).
         */
        std::array<std::uint16_t, Csm::kS> U_col{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @name U_diag
         * @brief Diagonal unknowns remaining (free cells per diag).
         */
        std::array<std::uint16_t, Csm::kS> U_diag{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /**
         * @name U_xdiag
         * @brief Anti-diagonal unknowns remaining (free cells per xdiag).
         */
        std::array<std::uint16_t, Csm::kS> U_xdiag{}; // NOLINT(misc-non-private-member-variables-in-classes)
    };
} // namespace crsce::decompress
