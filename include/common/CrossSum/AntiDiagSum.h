/**
 * @file AntiDiagSum.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Concrete CrossSumVector subtype for anti-diagonal sums (XSM).
 */
#pragma once

#include "common/CrossSum/CrossSumVector.h"

namespace crsce::common {

    /**
     * @name AntiDiagSum
     * @brief Anti-diagonal-sum (XSM) cross-sum vector. Line index is x = r + c.
     */
    class AntiDiagSum final : public CrossSumVector {
    public:
        /**
         * @name AntiDiagSum
         * @brief Construct an anti-diagonal-sum vector for a matrix of dimension s.
         * @param s The matrix dimension.
         */
        explicit AntiDiagSum(std::uint16_t s);

        /**
         * @name cells
         * @brief Return all cells on the same anti-diagonal as (r, c).
         * @param r Row index of the cell.
         * @param c Column index of the cell.
         * @return Vector of (row, column) pairs for the anti-diagonal.
         */
        [[nodiscard]] std::vector<std::pair<std::uint16_t, std::uint16_t>>
        cells(std::uint16_t r, std::uint16_t c) const override;

        /**
         * @name len
         * @brief Return the number of cells on anti-diagonal k.
         * @param k Anti-diagonal index.
         * @return min(k + 1, s, 2s - 1 - k).
         */
        [[nodiscard]] std::uint16_t len(std::uint16_t k) const override;

    protected:
        /**
         * @name lineIndex
         * @brief Map cell (r, c) to its anti-diagonal index: r + c.
         * @param r Row index.
         * @param c Column index.
         * @return r + c
         */
        [[nodiscard]] std::uint16_t lineIndex(std::uint16_t r, std::uint16_t c) const override;
    };

} // namespace crsce::common
