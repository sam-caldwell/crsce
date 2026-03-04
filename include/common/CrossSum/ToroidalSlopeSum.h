/**
 * @file ToroidalSlopeSum.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Concrete CrossSumVector subtype for toroidal-slope partition sums (HSM1/SFC1/HSM2/SFC2).
 */
#pragma once

#include "common/CrossSum/CrossSumVector.h"

#include <cstdint>

namespace crsce::common {

    /**
     * @class ToroidalSlopeSum
     * @name ToroidalSlopeSum
     * @brief Toroidal-slope cross-sum vector parameterized by slope p.
     *
     * Each partition has s lines of s cells. Line index for cell (r,c) with slope p:
     *   k = ((c - p * r) % s + s) % s
     * Cell enumeration for line k: (t, (k + p*t) % s) for t = 0..s-1.
     *
     * The four B.5 partitions use slopes: 256, 255, 2, 509.
     */
    class ToroidalSlopeSum final : public CrossSumVector {
    public:
        /**
         * @name ToroidalSlopeSum
         * @brief Construct a toroidal-slope sum vector for a matrix of dimension s with slope p.
         * @param s The matrix dimension.
         * @param slope The slope parameter p (e.g. 256, 255, 2, 509).
         */
        ToroidalSlopeSum(std::uint16_t s, std::uint16_t slope);

        /**
         * @name cells
         * @brief Return all cells on the same toroidal-slope line as (r, c).
         * @param r Row index of the cell.
         * @param c Column index of the cell.
         * @return Vector of (row, column) pairs for the line.
         */
        [[nodiscard]] std::vector<std::pair<std::uint16_t, std::uint16_t>>
        cells(std::uint16_t r, std::uint16_t c) const override;

        /**
         * @name len
         * @brief Return the number of cells on line k. For toroidal slopes, always s.
         * @param k Line index.
         * @return s
         */
        [[nodiscard]] std::uint16_t len(std::uint16_t k) const override;

        /**
         * @name slopeLineIndex
         * @brief Compute the toroidal-slope line index for cell (r, c).
         * @param r Row index.
         * @param c Column index.
         * @param slope The slope parameter p.
         * @param s The matrix dimension.
         * @return ((c - slope * r) % s + s) % s
         */
        [[nodiscard]] static constexpr std::uint16_t slopeLineIndex(
            const std::uint16_t r, const std::uint16_t c,
            const std::uint16_t slope, const std::uint16_t s) {
            // Use int32 to avoid unsigned underflow in the modular arithmetic.
            const auto si = static_cast<std::int32_t>(s);
            const auto val = static_cast<std::int32_t>(c)
                           - (static_cast<std::int32_t>(slope) * static_cast<std::int32_t>(r));
            return static_cast<std::uint16_t>(((val % si) + si) % si);
        }

    protected:
        /**
         * @name lineIndex
         * @brief Map cell (r, c) to its toroidal-slope line index: ((c - slope_ * r) % s_ + s_) % s_.
         * @param r Row index.
         * @param c Column index.
         * @return The line index for cell (r, c).
         */
        [[nodiscard]] std::uint16_t lineIndex(std::uint16_t r, std::uint16_t c) const override;

    private:
        /**
         * @name slope_
         * @brief The slope parameter p for this toroidal partition.
         */
        std::uint16_t slope_;
    };

} // namespace crsce::common
