/**
 * @file DiagSum.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Concrete CrossSumVector subtype for diagonal sums (DSM).
 */
#pragma once

#include "common/CrossSum/CrossSumVector.h"

namespace crsce::common {

    /**
     * @name DiagSum
     * @brief Diagonal-sum (DSM) cross-sum vector. Line index is d = c - r + (s - 1).
     */
    class DiagSum final : public CrossSumVector {
    public:
        /**
         * @name DiagSum
         * @brief Construct a diagonal-sum vector for a matrix of dimension s.
         * @param s The matrix dimension.
         */
        explicit DiagSum(std::uint16_t s);

        /**
         * @name cells
         * @brief Return all cells on the same diagonal as (r, c).
         * @param r Row index of the cell.
         * @param c Column index of the cell.
         * @return Vector of (row, column) pairs for the diagonal.
         */
        [[nodiscard]] std::vector<std::pair<std::uint16_t, std::uint16_t>>
        cells(std::uint16_t r, std::uint16_t c) const override;

        /**
         * @name len
         * @brief Return the number of cells on diagonal k.
         * @param k Diagonal index.
         * @return min(k + 1, s, 2s - 1 - k).
         */
        [[nodiscard]] std::uint16_t len(std::uint16_t k) const override;

    protected:
        /**
         * @name lineIndex
         * @brief Map cell (r, c) to its diagonal index: c - r + (s - 1).
         * @param r Row index.
         * @param c Column index.
         * @return c - r + (s - 1)
         */
        [[nodiscard]] std::uint16_t lineIndex(std::uint16_t r, std::uint16_t c) const override;
    };

} // namespace crsce::common
