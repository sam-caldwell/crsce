/**
 * @file CrossSumVector.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Abstract base class for cross-sum vectors used in CSM reconstruction.
 */
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace crsce::common {

    /**
     * @name CrossSumVector
     * @brief Abstract base type representing an ordered sequence of integer sums along one family of lines
     *        (row, column, diagonal, or anti-diagonal) through a square bit matrix of dimension s.
     */
    class CrossSumVector {
    public:
        /**
         * @name CrossSumVector
         * @brief Construct a cross-sum vector for a matrix of dimension s.
         * @param s The matrix dimension (number of rows and columns).
         */
        explicit CrossSumVector(std::uint16_t s);

        CrossSumVector(const CrossSumVector &) = delete;
        CrossSumVector &operator=(const CrossSumVector &) = delete;
        CrossSumVector(CrossSumVector &&) = delete;
        CrossSumVector &operator=(CrossSumVector &&) = delete;

        /**
         * @name ~CrossSumVector
         * @brief Virtual destructor.
         */
        virtual ~CrossSumVector() = default;

        /**
         * @name set
         * @brief Accumulate a cell's value into the appropriate sum for the line containing cell (r, c).
         * @param r Row index of the cell.
         * @param c Column index of the cell.
         * @param v Value to accumulate (typically 0 or 1).
         */
        void set(std::uint16_t r, std::uint16_t c, std::uint8_t v);

        /**
         * @name get
         * @brief Retrieve the current sum for the line containing cell (r, c).
         * @param r Row index of the cell.
         * @param c Column index of the cell.
         * @return The sum for the line containing (r, c).
         */
        [[nodiscard]] std::uint16_t get(std::uint16_t r, std::uint16_t c) const;

        /**
         * @name cells
         * @brief Return the set of (r, c) indices participating in the same line as the given cell.
         * @param r Row index of the cell.
         * @param c Column index of the cell.
         * @return Vector of (row, column) pairs on the same line.
         */
        [[nodiscard]] virtual std::vector<std::pair<std::uint16_t, std::uint16_t>>
        cells(std::uint16_t r, std::uint16_t c) const = 0;

        /**
         * @name len
         * @brief Return the number of cells on line k.
         * @param k Line index.
         * @return Number of cells participating in line k.
         */
        [[nodiscard]] virtual std::uint16_t len(std::uint16_t k) const = 0;

        /**
         * @name size
         * @brief Return the number of elements (lines) in this cross-sum vector.
         * @return The number of lines.
         */
        [[nodiscard]] std::uint16_t size() const;

        /**
         * @name getByIndex
         * @brief Direct index access for serialization: retrieve the sum at line index k.
         * @param k Line index.
         * @return The sum stored at index k.
         */
        [[nodiscard]] std::uint16_t getByIndex(std::uint16_t k) const;

        /**
         * @name setByIndex
         * @brief Direct index access for serialization: set the sum at line index k.
         * @param k Line index.
         * @param value The value to store at index k.
         */
        void setByIndex(std::uint16_t k, std::uint16_t value);

    protected:
        /**
         * @name lineIndex
         * @brief Map a cell coordinate (r, c) to its line index in this cross-sum vector.
         * @param r Row index of the cell.
         * @param c Column index of the cell.
         * @return The line index for cell (r, c).
         */
        [[nodiscard]] virtual std::uint16_t lineIndex(std::uint16_t r, std::uint16_t c) const = 0;

        /**
         * @name s_
         * @brief The matrix dimension (number of rows and columns).
         */
        std::uint16_t s_; // NOLINT(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)

        /**
         * @name sums_
         * @brief The underlying storage for cross-sum values, one per line.
         */
        std::vector<std::uint16_t> sums_; // NOLINT(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
    };

} // namespace crsce::common
