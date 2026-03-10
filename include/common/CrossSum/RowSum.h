/**
 * @file RowSum.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Concrete CrossSumVector subtype for row sums (LSM).
 */
#pragma once

#include "common/CrossSum/CrossSumVector.h"

namespace crsce::common {

    /**
     * @name RowSum
     * @brief Row-sum (Lateral Sum, LSM) cross-sum vector. Line index is the row index r.
     */
    class RowSum final : public CrossSumVector {
    public:
        /**
         * @name RowSum
         * @brief Construct a row-sum vector for a matrix of dimension s.
         * @param s The matrix dimension.
         */
        explicit RowSum(std::uint16_t s);

        /**
         * @name cells
         * @brief Return all cells in the same row as (r, c).
         * @param r Row index of the cell.
         * @param c Column index of the cell.
         * @return Vector of (row, column) pairs for the entire row.
         */
        [[nodiscard]] std::vector<std::pair<std::uint16_t, std::uint16_t>>
        cells(std::uint16_t r, std::uint16_t c) const override;

        /**
         * @name len
         * @brief Return the number of cells on line k. For row sums, this is always s.
         * @param k Line index.
         * @return The number of cells on line k.
         */
        [[nodiscard]] std::uint16_t len(std::uint16_t k) const override;

    protected:
        /**
         * @name lineIndex
         * @brief Map cell (r, c) to its line index. For row sums, returns r.
         * @param r Row index.
         * @param c Column index (unused).
         * @return r
         */
        [[nodiscard]] std::uint16_t lineIndex(std::uint16_t r, std::uint16_t c) const override;
    };

} // namespace crsce::common
