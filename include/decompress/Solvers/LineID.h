/**
 * @file LineID.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Opaque identifier for a constraint line (row, column, diagonal, anti-diagonal, LTP).
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @enum LineType
     * @brief Discriminator for the ten families of cross-sum constraints.
     *
     * B.20: replaced 4 toroidal-slope families (Slope256/255/2/509) with 4 pseudorandom
     * LTP partitions (LTP1–LTP4), keeping the same total of 10s-2 = 5108 lines.
     * B.27: added LTP5 and LTP6 (12s-2 = 6130 lines total, 10 lines per cell).
     *
     * // B.20 disabled (original slope values, retained for reference):
     * // Slope256 = 4,  Slope255 = 5,  Slope2 = 6,  Slope509 = 7,  LTP1 = 8,  LTP2 = 9
     */
    enum class LineType : std::uint8_t {
        Row = 0,
        Column = 1,
        Diagonal = 2,
        AntiDiagonal = 3,
        LTP1 = 4,
        LTP2 = 5,
        LTP3 = 6,
        LTP4 = 7,
        LTP5 = 8,
        LTP6 = 9
    };

    /**
     * @struct LineID
     * @name LineID
     * @brief Opaque identifier for a specific line within the constraint system.
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct LineID {
        /**
         * @name type
         * @brief Which constraint family this line belongs to.
         */
        LineType type;

        /**
         * @name index
         * @brief Index within the family (0..s-1 for row/col, 0..2s-2 for diag/anti-diag).
         */
        std::uint16_t index;

        /**
         * @name operator==
         * @brief Equality comparison.
         * @param other The other LineID to compare against.
         * @return True if both type and index match.
         */
        bool operator==(const LineID &other) const = default;
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)
} // namespace crsce::decompress::solvers
