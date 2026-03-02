/**
 * @file LineID.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Opaque identifier for a constraint line (row, column, diagonal, anti-diagonal).
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @enum LineType
     * @brief Discriminator for the four families of cross-sum constraints.
     */
    enum class LineType : std::uint8_t {
        Row = 0,
        Column = 1,
        Diagonal = 2,
        AntiDiagonal = 3
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
