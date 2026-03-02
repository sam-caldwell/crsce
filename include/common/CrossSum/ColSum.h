/**
 * @file ColSum.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Concrete CrossSumVector subtype for column sums (VSM).
 */
#pragma once

#include "common/CrossSum/CrossSumVector.h"

namespace crsce::common {

    /**
     * @name ColSum
     * @brief Column-sum (Vertical Sum, VSM) cross-sum vector. Line index is the column index c.
     */
    class ColSum final : public CrossSumVector {
    public:
        /**
         * @name ColSum
         * @brief Construct a column-sum vector for a matrix of dimension s.
         * @param s The matrix dimension.
         */
        explicit ColSum(std::uint16_t s);

        /**
         * @name cells
         * @brief Return all cells in the same column as (r, c).
         * @param r Row index of the cell.
         * @param c Column index of the cell.
         * @return Vector of (row, column) pairs for the entire column.
         */
        [[nodiscard]] std::vector<std::pair<std::uint16_t, std::uint16_t>>
        cells(std::uint16_t r, std::uint16_t c) const override;

        /**
         * @name len
         * @brief Return the number of cells on line k. For column sums, this is always s.
         * @param k Line index.
         * @return The number of cells on line k.
         */
        [[nodiscard]] std::uint16_t len(std::uint16_t k) const override;

    protected:
        /**
         * @name lineIndex
         * @brief Map cell (r, c) to its line index. For column sums, returns c.
         * @param r Row index (unused).
         * @param c Column index.
         * @return c
         */
        [[nodiscard]] std::uint16_t lineIndex(std::uint16_t r, std::uint16_t c) const override;
    };

} // namespace crsce::common
